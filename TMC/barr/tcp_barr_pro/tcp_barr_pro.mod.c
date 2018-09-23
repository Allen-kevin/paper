#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x348ba56c, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x4067890e, __VMLINUX_SYMBOL_STR(tcp_unregister_retransmission_algorithm) },
	{ 0x631c1cf0, __VMLINUX_SYMBOL_STR(tcp_register_retransmission_algorithm) },
	{ 0xb54533f7, __VMLINUX_SYMBOL_STR(usecs_to_jiffies) },
	{ 0xac82a787, __VMLINUX_SYMBOL_STR(sysctl_tcp_queue_threshold) },
	{ 0x3a698aba, __VMLINUX_SYMBOL_STR(sysctl_tcp_temp_threshold) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "26077D2AEDA933C82AECEAA");
