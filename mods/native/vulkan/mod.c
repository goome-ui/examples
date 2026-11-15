#define GOOME_IMPL_WIN32_IMPLEMENTATION
#define GOOME_IMPL_VULKAN_IMPLEMENTATION
#include "goome_vulkan.h"
#include "goome_win32.h"

#include "mod.h"

static PFN_vkGetInstanceProcAddr gipa;
static PFN_vkGetDeviceProcAddr gdpa;
static VkInstance inst;
static VkPhysicalDevice phys;
static VkDevice dvc;
static uint32_t qfam = ~0u;
static VkQueue gq;
static int ready;

typedef VkResult(VKAPI_PTR *present_t)(VkQueue, const VkPresentInfoKHR *);
typedef VkResult(VKAPI_PTR *create_sw_t)(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *);
typedef VkResult(VKAPI_PTR *create_dev_t)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *, VkDevice *);
typedef VkResult(VKAPI_PTR *create_inst_t)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *);
static present_t real_present;
static create_sw_t real_create_sw;
static create_dev_t real_create_dev;
static create_inst_t real_create_inst;

static struct {
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
    PFN_vkCmdEndRenderPass CmdEndRenderPass;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkGetDeviceQueue GetDeviceQueue;
} vk;

static struct {
    VkSwapchainKHR handle;
    VkFormat fmt;
    VkExtent2D ext;
    uint32_t n;
    VkImageView views[8];
    VkFramebuffer fbs[8];
    VkRenderPass rp;
    VkCommandPool pool;
    VkCommandBuffer cbs[8];
    VkSemaphore sems[8];
} sw;

static void resolve(void)
{
    if (vk.QueueSubmit || !gdpa || !dvc) return;
#define G(n) vk.n = (PFN_vk##n)gdpa(dvc, "vk" #n)
    G(GetSwapchainImagesKHR), G(CreateImageView), G(DestroyImageView), G(CreateRenderPass), G(DestroyRenderPass);
    G(CreateFramebuffer), G(DestroyFramebuffer), G(CreateCommandPool), G(DestroyCommandPool), G(AllocateCommandBuffers);
    G(BeginCommandBuffer), G(EndCommandBuffer), G(ResetCommandBuffer), G(CmdBeginRenderPass), G(CmdEndRenderPass);
    G(QueueSubmit), G(CreateSemaphore), G(DestroySemaphore), G(DeviceWaitIdle), G(GetDeviceQueue);
#undef G
}

static void drop_sw(void)
{
    if (!dvc) return;
    vk.DeviceWaitIdle(dvc);
    for (uint32_t i = 0; i < sw.n; i++) {
        if (sw.fbs[i]) vk.DestroyFramebuffer(dvc, sw.fbs[i], NULL);
        if (sw.views[i]) vk.DestroyImageView(dvc, sw.views[i], NULL);
        if (sw.sems[i]) vk.DestroySemaphore(dvc, sw.sems[i], NULL);
    }
    if (sw.rp) vk.DestroyRenderPass(dvc, sw.rp, NULL);
    if (sw.pool) vk.DestroyCommandPool(dvc, sw.pool, NULL);
    memset(&sw, 0, sizeof(sw));
}

static int build_sw(VkSwapchainKHR handle)
{
    resolve();
    if (!vk.QueueSubmit) return 0;
    if (sw.handle == handle && sw.rp) return 1;
    drop_sw();
    sw.handle = handle;
    VkImage imgs[8];
    uint32_t n = 8;
    if (vk.GetSwapchainImagesKHR(dvc, handle, &n, imgs) < 0 || n == 0) return 0;
    if (n > 8) n = 8;
    sw.n = n;
    VkAttachmentDescription at;
    memset(&at, 0, sizeof(at));
    at.format = sw.fmt, at.samples = VK_SAMPLE_COUNT_1_BIT, at.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    at.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, at.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    at.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, at.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub;
    memset(&sub, 0, sizeof(sub));
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, sub.colorAttachmentCount = 1, sub.pColorAttachments = &ref;
    VkRenderPassCreateInfo rp = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &at, 1, &sub, 0, NULL};
    if (vk.CreateRenderPass(dvc, &rp, NULL, &sw.rp) != VK_SUCCESS) return 0;
    VkCommandPoolCreateInfo pc = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qfam};
    if (vk.CreateCommandPool(dvc, &pc, NULL, &sw.pool) != VK_SUCCESS) return 0;
    VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, sw.pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, n};
    if (vk.AllocateCommandBuffers(dvc, &ca, sw.cbs) != VK_SUCCESS) return 0;
    for (uint32_t i = 0; i < n; i++) {
        VkImageViewCreateInfo vc;
        memset(&vc, 0, sizeof(vc));
        vc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, vc.image = imgs[i], vc.viewType = VK_IMAGE_VIEW_TYPE_2D, vc.format = sw.fmt;
        vc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, vc.subresourceRange.levelCount = 1, vc.subresourceRange.layerCount = 1;
        if (vk.CreateImageView(dvc, &vc, NULL, &sw.views[i]) != VK_SUCCESS) return 0;
        VkFramebufferCreateInfo fc = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, sw.rp, 1, &sw.views[i], sw.ext.width, sw.ext.height, 1};
        if (vk.CreateFramebuffer(dvc, &fc, NULL, &sw.fbs[i]) != VK_SUCCESS) return 0;
        VkSemaphoreCreateInfo se = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0};
        if (vk.CreateSemaphore(dvc, &se, NULL, &sw.sems[i]) != VK_SUCCESS) return 0;
    }
    if (!gq) vk.GetDeviceQueue(dvc, qfam, 0, &gq);
    goome_vulkan_info in;
    memset(&in, 0, sizeof(in));
    in.instance = inst, in.physical_device = phys, in.device = dvc, in.queue_family = qfam, in.queue = gq;
    in.render_pass = sw.rp, in.subpass = 0, in.samples = VK_SAMPLE_COUNT_1_BIT, in.get_proc = gipa;
    ready = goome_vulkan_init(gmod.g, &in) == GOOME_OK;
    return ready;
}

static void overlay(VkQueue q, const VkPresentInfoKHR *info, uint32_t i, VkSemaphore *out)
{
    uint32_t img = info->pImageIndices[i];
    if (img >= sw.n) return;
    vk.ResetCommandBuffer(sw.cbs[img], 0);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL};
    vk.BeginCommandBuffer(sw.cbs[img], &bi);
    VkRenderPassBeginInfo rb;
    memset(&rb, 0, sizeof(rb));
    rb.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, rb.renderPass = sw.rp, rb.framebuffer = sw.fbs[img];
    rb.renderArea.extent = sw.ext;
    vk.CmdBeginRenderPass(sw.cbs[img], &rb, VK_SUBPASS_CONTENTS_INLINE);
    goome_mod_frame("Vulkan");
    goome_vulkan_render(gmod.g, sw.cbs[img]);
    vk.CmdEndRenderPass(sw.cbs[img]);
    vk.EndCommandBuffer(sw.cbs[img]);
    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = info->waitSemaphoreCount, si.pWaitSemaphores = info->pWaitSemaphores, si.pWaitDstStageMask = &wait;
    si.commandBufferCount = 1, si.pCommandBuffers = &sw.cbs[img];
    si.signalSemaphoreCount = 1, si.pSignalSemaphores = &sw.sems[img];
    if (vk.QueueSubmit(q, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS) *out = sw.sems[img];
}

static VkResult VKAPI_PTR my_present(VkQueue q, const VkPresentInfoKHR *info)
{
    HWND win = GetActiveWindow();
    if (win && goome_mod_ensure((int)sw.ext.width, (int)sw.ext.height)) goome_mod_subclass(win);
    VkSemaphore sem = VK_NULL_HANDLE;
    if (sw.ext.width && info->swapchainCount && build_sw(info->pSwapchains[0])) overlay(q, info, 0, &sem);
    if (sem) {
        VkPresentInfoKHR p = *info;
        p.waitSemaphoreCount = 1, p.pWaitSemaphores = &sem;
        return real_present(q, &p);
    }
    return real_present(q, info);
}

static VkResult VKAPI_PTR my_create_sw(VkDevice d, const VkSwapchainCreateInfoKHR *ci, const VkAllocationCallbacks *a, VkSwapchainKHR *out)
{
    VkSwapchainCreateInfoKHR c = *ci;
    c.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkResult r = real_create_sw(d, &c, a, out);
    if (r == VK_SUCCESS) {
        drop_sw();
        sw.fmt = c.imageFormat, sw.ext = c.imageExtent, sw.handle = VK_NULL_HANDLE;
    }
    return r;
}

static VkResult VKAPI_PTR my_create_dev(VkPhysicalDevice pd, const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *a, VkDevice *out)
{
    VkResult r = real_create_dev(pd, ci, a, out);
    if (r == VK_SUCCESS) {
        phys = pd, dvc = *out;
        if (ci->queueCreateInfoCount) qfam = ci->pQueueCreateInfos[0].queueFamilyIndex;
    }
    return r;
}

static VkResult VKAPI_PTR my_create_inst(const VkInstanceCreateInfo *ci, const VkAllocationCallbacks *a, VkInstance *out)
{
    VkResult r = real_create_inst(ci, a, out);
    if (r == VK_SUCCESS) inst = *out;
    return r;
}

int goome_mod_start(void)
{
    HMODULE lib = GetModuleHandleW(L"vulkan-1.dll");
    if (!lib) lib = LoadLibraryW(L"vulkan-1.dll");
    if (!lib) return 0;
    gipa = (PFN_vkGetInstanceProcAddr)(void (*)(void))GetProcAddress(lib, "vkGetInstanceProcAddr");
    gdpa = (PFN_vkGetDeviceProcAddr)(void (*)(void))GetProcAddress(lib, "vkGetDeviceProcAddr");
    if (!gipa || MH_Initialize() != MH_OK) return 0;
    MH_CreateHookApi(L"vulkan-1", "vkCreateInstance", (void *)my_create_inst, (void **)&real_create_inst);
    MH_CreateHookApi(L"vulkan-1", "vkCreateDevice", (void *)my_create_dev, (void **)&real_create_dev);
    MH_CreateHookApi(L"vulkan-1", "vkCreateSwapchainKHR", (void *)my_create_sw, (void **)&real_create_sw);
    MH_CreateHookApi(L"vulkan-1", "vkQueuePresentKHR", (void *)my_present, (void **)&real_present);
    MH_EnableHook(MH_ALL_HOOKS);
    return 1;
}

void goome_mod_stop(void)
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    if (ready) {
        drop_sw();
        goome_vulkan_shutdown(gmod.g);
    }
    goome_mod_teardown();
}
