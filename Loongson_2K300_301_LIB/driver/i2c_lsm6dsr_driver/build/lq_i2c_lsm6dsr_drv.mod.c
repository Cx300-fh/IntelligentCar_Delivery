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
	{ 0xe3ae0b4d, "module_layout" },
	{ 0x2d3385d3, "system_wq" },
	{ 0xd239f36b, "cdev_del" },
	{ 0xc382d164, "cdev_init" },
	{ 0x2b68bd2f, "del_timer" },
	{ 0x43a53735, "__alloc_workqueue_key" },
	{ 0x751d9827, "i2c_del_driver" },
	{ 0x8af7b5a1, "i2c_transfer" },
	{ 0x72d2e962, "device_destroy" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x526c3a6c, "jiffies" },
	{ 0xe526f136, "__copy_user" },
	{ 0x7c32d0f0, "printk" },
	{ 0x8c03d20c, "destroy_workqueue" },
	{ 0x368ad888, "device_create" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0x24d273d1, "add_timer" },
	{ 0x42160169, "flush_workqueue" },
	{ 0xb7a2cd76, "i2c_register_driver" },
	{ 0x772a8264, "cdev_add" },
	{ 0x4fcf188b, "memcpy" },
	{ 0xe07aac8e, "class_destroy" },
	{ 0x2e0d2f7f, "queue_work_on" },
	{ 0x33f7c8d4, "devm_kmalloc" },
	{ 0x4c989a74, "__class_create" },
	{ 0x9e7d6bd0, "__udelay" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "4DFC337186F6AB33600B2DF");
