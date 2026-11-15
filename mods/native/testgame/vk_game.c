#include "common.h"

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#undef CreateSemaphore

static PFN_vkGetInstanceProcAddr gipa;
static PFN_vkGetDeviceProcAddr gdpa;
static VkInstance inst;
static VkPhysicalDevice phys;
static VkDevice dev;
static VkQueue queue;
static uint32_t qfam;
static VkSurfaceKHR surf;
static VkSwapchainKHR swap;
static VkFormat fmt;
static VkImage images[8];
static uint32_t nimg;
static VkCommandPool pool;
static VkCommandBuffer cbs[8];
static VkSemaphore acq, done;
static VkBuffer readbuf;
static VkDeviceMemory readmem;
static int flip;

#define IFN(n) static PFN_vk##n n
#define DFN(n) static PFN_vk##n n
IFN(CreateInstance);
IFN(GetPhysicalDeviceQueueFamilyProperties);
IFN(EnumeratePhysicalDevices);
IFN(CreateWin32SurfaceKHR);
IFN(GetPhysicalDeviceSurfaceSupportKHR);
IFN(GetPhysicalDeviceSurfaceFormatsKHR);
IFN(CreateDevice);
IFN(GetPhysicalDeviceMemoryProperties);
DFN(GetDeviceQueue);
DFN(CreateSwapchainKHR);
DFN(GetSwapchainImagesKHR);
DFN(CreateCommandPool);
DFN(AllocateCommandBuffers);
DFN(BeginCommandBuffer);
DFN(EndCommandBuffer);
DFN(CmdPipelineBarrier);
DFN(CmdClearColorImage);
DFN(CmdCopyImageToBuffer);
DFN(CreateSemaphore);
DFN(AcquireNextImageKHR);
DFN(QueueSubmit);
DFN(QueuePresentKHR);
DFN(QueueWaitIdle);
DFN(CreateBuffer);
DFN(GetBufferMemoryRequirements);
DFN(AllocateMemory);
DFN(BindBufferMemory);
DFN(MapMemory);
DFN(ResetCommandBuffer);

static uint32_t cur;

static void record(uint32_t i, VkImageLayout to, VkAccessFlags dstacc, int clear)
{
    ResetCommandBuffer(cbs[i], 0);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL};
    BeginCommandBuffer(cbs[i], &bi);
    VkImageMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, b.newLayout = clear ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, b.image = images[i];
    b.dstAccessMask = clear ? VK_ACCESS_TRANSFER_WRITE_BIT : dstacc;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, b.subresourceRange.levelCount = 1, b.subresourceRange.layerCount = 1;
    CmdPipelineBarrier(cbs[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
    if (clear) {
        VkClearColorValue cc;
        cc.float32[0] = tg_clear[0] / 255.0f, cc.float32[1] = tg_clear[1] / 255.0f, cc.float32[2] = tg_clear[2] / 255.0f, cc.float32[3] = 1;
        VkImageSubresourceRange sr = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        CmdClearColorImage(cbs[i], images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cc, 1, &sr);
        VkImageMemoryBarrier b2 = b;
        b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, b2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, b2.dstAccessMask = 0;
        CmdPipelineBarrier(cbs[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &b2);
    }
    EndCommandBuffer(cbs[i]);
}

static void frame(void)
{
    if (AcquireNextImageKHR(dev, swap, UINT64_MAX, acq, VK_NULL_HANDLE, &cur) != VK_SUCCESS) return;
    record(cur, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 1);
    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, si.waitSemaphoreCount = 1, si.pWaitSemaphores = &acq, si.pWaitDstStageMask = &wait;
    si.commandBufferCount = 1, si.pCommandBuffers = &cbs[cur], si.signalSemaphoreCount = 1, si.pSignalSemaphores = &done;
    QueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    VkPresentInfoKHR pi;
    memset(&pi, 0, sizeof(pi));
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, pi.waitSemaphoreCount = 1, pi.pWaitSemaphores = &done;
    pi.swapchainCount = 1, pi.pSwapchains = &swap, pi.pImageIndices = &cur;
    QueuePresentKHR(queue, &pi);
    QueueWaitIdle(queue);
}

static int readback(unsigned char *rgba)
{
    uint32_t pitch = TG_W * 4;
    ResetCommandBuffer(cbs[cur], 0);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL};
    BeginCommandBuffer(cbs[cur], &bi);
    VkImageMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, b.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, b.image = images[cur], b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, b.subresourceRange.levelCount = 1, b.subresourceRange.layerCount = 1;
    CmdPipelineBarrier(cbs[cur], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
    VkBufferImageCopy c;
    memset(&c, 0, sizeof(c));
    c.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, c.imageSubresource.layerCount = 1;
    c.imageExtent.width = TG_W, c.imageExtent.height = TG_H, c.imageExtent.depth = 1;
    CmdCopyImageToBuffer(cbs[cur], images[cur], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbuf, 1, &c);
    EndCommandBuffer(cbs[cur]);
    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, si.commandBufferCount = 1, si.pCommandBuffers = &cbs[cur];
    QueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    QueueWaitIdle(queue);
    void *p = NULL;
    if (MapMemory(dev, readmem, 0, VK_WHOLE_SIZE, 0, &p) != VK_SUCCESS || !p) return 0;
    const unsigned char *s = p;
    for (int i = 0; i < TG_W * TG_H; i++) {
        unsigned char *o = rgba + (size_t)i * 4;
        if (flip) o[0] = s[i * 4 + 2], o[1] = s[i * 4 + 1], o[2] = s[i * 4 + 0];
        else o[0] = s[i * 4 + 0], o[1] = s[i * 4 + 1], o[2] = s[i * 4 + 2];
        o[3] = 255;
    }
    (void)pitch;
    return 1;
}

static int load(void)
{
    HMODULE lib = LoadLibraryW(L"vulkan-1.dll");
    if (!lib) return 0;
    gipa = (PFN_vkGetInstanceProcAddr)(void (*)(void))GetProcAddress(lib, "vkGetInstanceProcAddr");
    if (!gipa) return 0;
    CreateInstance = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
    const char *ext[2] = {"VK_KHR_surface", "VK_KHR_win32_surface"};
    VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "tg", 0, "tg", 0, VK_API_VERSION_1_0};
    VkInstanceCreateInfo ic;
    memset(&ic, 0, sizeof(ic));
    ic.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, ic.pApplicationInfo = &ai, ic.enabledExtensionCount = 2, ic.ppEnabledExtensionNames = ext;
    if (CreateInstance(&ic, NULL, &inst) != VK_SUCCESS) return 0;
#define GI(n) n = (PFN_vk##n)gipa(inst, "vk" #n)
    GI(EnumeratePhysicalDevices), GI(GetPhysicalDeviceQueueFamilyProperties), GI(CreateWin32SurfaceKHR), GI(GetPhysicalDeviceSurfaceSupportKHR);
    GI(GetPhysicalDeviceSurfaceFormatsKHR), GI(CreateDevice), GI(GetPhysicalDeviceMemoryProperties);
    gdpa = (PFN_vkGetDeviceProcAddr)gipa(inst, "vkGetDeviceProcAddr");
#undef GI
    uint32_t n = 1;
    if (EnumeratePhysicalDevices(inst, &n, &phys) < 0 || !n) return 0;
    return 1;
}

static int memtype(uint32_t bits, VkMemoryPropertyFlags want)
{
    VkPhysicalDeviceMemoryProperties mp;
    GetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return (int)i;
    return -1;
}

int main(void)
{
    HWND win = tg_make_window(L"goome_tg_vk");
    if (!win || !load()) return printf("vulkan not available, skipped\n"), 0;
    VkWin32SurfaceCreateInfoKHR sci = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, NULL, 0, GetModuleHandleW(NULL), win};
    if (CreateWin32SurfaceKHR(inst, &sci, NULL, &surf) != VK_SUCCESS) return printf("no surface, skipped\n"), 0;
    uint32_t nq = 0;
    GetPhysicalDeviceQueueFamilyProperties(phys, &nq, NULL);
    VkQueueFamilyProperties qp[16];
    if (nq > 16) nq = 16;
    GetPhysicalDeviceQueueFamilyProperties(phys, &nq, qp);
    qfam = ~0u;
    for (uint32_t i = 0; i < nq; i++) {
        VkBool32 ok = 0;
        GetPhysicalDeviceSurfaceSupportKHR(phys, i, surf, &ok);
        if ((qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && ok) {
            qfam = i;
            break;
        }
    }
    if (qfam == ~0u) return printf("no present queue, skipped\n"), 0;
    float prio = 1;
    VkDeviceQueueCreateInfo qc = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, qfam, 1, &prio};
    const char *dext[1] = {"VK_KHR_swapchain"};
    VkDeviceCreateInfo dc;
    memset(&dc, 0, sizeof(dc));
    dc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, dc.queueCreateInfoCount = 1, dc.pQueueCreateInfos = &qc, dc.enabledExtensionCount = 1, dc.ppEnabledExtensionNames = dext;
    if (CreateDevice(phys, &dc, NULL, &dev) != VK_SUCCESS) return printf("no device, skipped\n"), 0;
#define GD(n) n = (PFN_vk##n)gdpa(dev, "vk" #n)
    GD(GetDeviceQueue), GD(CreateSwapchainKHR), GD(GetSwapchainImagesKHR), GD(CreateCommandPool), GD(AllocateCommandBuffers);
    GD(BeginCommandBuffer), GD(EndCommandBuffer), GD(CmdPipelineBarrier), GD(CmdClearColorImage), GD(CmdCopyImageToBuffer);
    GD(CreateSemaphore), GD(AcquireNextImageKHR), GD(QueueSubmit), GD(QueuePresentKHR), GD(QueueWaitIdle);
    GD(CreateBuffer), GD(GetBufferMemoryRequirements), GD(AllocateMemory), GD(BindBufferMemory), GD(MapMemory), GD(ResetCommandBuffer);
#undef GD
    GetDeviceQueue(dev, qfam, 0, &queue);
    uint32_t nf = 1;
    VkSurfaceFormatKHR sf[8];
    GetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &nf, NULL);
    if (nf > 8) nf = 8;
    GetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &nf, sf);
    fmt = nf ? sf[0].format : VK_FORMAT_B8G8R8A8_UNORM;
    flip = (fmt == VK_FORMAT_B8G8R8A8_UNORM || fmt == VK_FORMAT_B8G8R8A8_SRGB);
    VkSwapchainCreateInfoKHR sc;
    memset(&sc, 0, sizeof(sc));
    sc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, sc.surface = surf, sc.minImageCount = 2, sc.imageFormat = fmt;
    sc.imageColorSpace = nf ? sf[0].colorSpace : 0, sc.imageExtent.width = TG_W, sc.imageExtent.height = TG_H, sc.imageArrayLayers = 1;
    sc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    sc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, sc.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    sc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, sc.presentMode = VK_PRESENT_MODE_FIFO_KHR, sc.clipped = VK_TRUE;
    if (CreateSwapchainKHR(dev, &sc, NULL, &swap) != VK_SUCCESS) return printf("no swapchain, skipped\n"), 0;
    nimg = 8;
    GetSwapchainImagesKHR(dev, swap, &nimg, images);
    VkCommandPoolCreateInfo pc = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qfam};
    CreateCommandPool(dev, &pc, NULL, &pool);
    VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nimg};
    AllocateCommandBuffers(dev, &ca, cbs);
    VkSemaphoreCreateInfo se = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0};
    CreateSemaphore(dev, &se, NULL, &acq);
    CreateSemaphore(dev, &se, NULL, &done);
    VkBufferCreateInfo bc = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, (VkDeviceSize)TG_W * TG_H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL};
    CreateBuffer(dev, &bc, NULL, &readbuf);
    VkMemoryRequirements mr;
    GetBufferMemoryRequirements(dev, readbuf, &mr);
    int mt = memtype(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo ma = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mr.size, (uint32_t)mt};
    AllocateMemory(dev, &ma, NULL, &readmem);
    BindBufferMemory(dev, readbuf, readmem, 0);
    int fails = tg_run("Vulkan", frame, readback);
    return fails != 0;
}
