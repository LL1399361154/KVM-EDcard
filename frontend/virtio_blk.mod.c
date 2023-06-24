#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xcf905263, "module_layout" },
	{ 0x992085cd, "unregister_virtio_driver" },
	{ 0xadd7ad9d, "register_virtio_driver" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x1000e51, "schedule" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0x3e0c4dff, "virtqueue_kick" },
	{ 0xf67efc3d, "virtqueue_add_sgs" },
	{ 0xb320cc0e, "sg_init_one" },
	{ 0xc3762aec, "mempool_alloc" },
	{ 0xb44ad4b3, "_copy_to_user" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x1953c958, "mempool_create" },
	{ 0xd35a6d31, "mempool_kmalloc" },
	{ 0x6a037cf1, "mempool_kfree" },
	{ 0x977f511b, "__mutex_init" },
	{ 0xf888ca21, "sg_init_table" },
	{ 0x85bcfdf2, "cdev_add" },
	{ 0x2ab3c0a9, "cdev_init" },
	{ 0x3d480beb, "device_create" },
	{ 0x9a068b04, "__class_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xb8b9f817, "kmalloc_order_trace" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0xc9da61ba, "preempt_voluntary_key" },
	{ 0x8f52a939, "virtio_check_driver_offered_feature" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0xa897e3e7, "mempool_free" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0xe284f179, "virtqueue_get_buf" },
	{ 0x37a0cba, "kfree" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xf67beee5, "cdev_del" },
	{ 0x9034a696, "mempool_destroy" },
	{ 0x2ea2c95c, "__x86_indirect_thunk_rax" },
	{ 0x409bcb62, "mutex_unlock" },
	{ 0x2ab7989d, "mutex_lock" },
	{ 0xc5850110, "printk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("virtio:d00000015v*");

MODULE_INFO(srcversion, "EC94C3B9F215BEF2520B88E");

MODULE_INFO(suserelease, "SLE15-SP3");
