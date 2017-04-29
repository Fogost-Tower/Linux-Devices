/*reference 
*drivers/mtd/nand/s3c2410.c
*drivers/mtd/nand/at91_nand.c
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

//#include <asm/arch/regs-nand.h>
//#include <asm/arch/nand.h>

typedef struct 
{
	ulong nfconf	;
	ulong nfcont	;
	ulong nfcmd	;
	ulong nfaddr	;
	ulong nfdata 	;
	ulong nfmecc0_0;	 
	ulong nfmecc1_0;
	ulong nfsecc_0   ;
	ulong nfstat 	;
	ulong nfestat0;
	ulong nfestat1; 
	ulong nfmecc0_1;
	ulong nfmecc1_1;
	ulong nfsecc_1   ;
	ulong nfsblk 	;
	ulong nfeblk	;
}s3c_nand_regs;


static struct nand_chip *s3c_chip;
static struct mtd_info *s3c_mtd;
static s3c_nand_regs *nand_reg_p;

static struct mtd_partition s3c_nand_part[] = {
	[0] = {
		.name 	= "bootloader",
		.size   	= 0x00040000,
		.offset 	= 0,
	},

	[1] = {
		.name 	= "params",
		.offset   	= MTDPART_OFS_APPEND,
		.size		= 0x00020000,
	},

	[2] = {
		.name	= "kernel",
		.offset 	= MTDPART_OFS_APPEND,
		.size 	= 0x00200000,
	},
	
	[3] = {
		.name	= "root",
		.offset 	= MTDPART_OFS_APPEND,
		.size 	= MTDPART_SIZ_FULL,
	}
};



static void s3c_select_chip(struct mtd_info *mtd, int chipnr)
{
	if(chipnr == -1)
	{
		//enable select 
		nand_reg_p->nfcont |= (1<<1);
	}
	else
	{	
		//disable select
		nand_reg_p->nfcont &= ~(1<<1); 
	}
}

static void s3c_nand_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	if (ctrl & NAND_CLE)
	{
		/*send command : NFCMMD = dat */
		nand_reg_p->nfcmd = dat;
	}
	else
	{
		/*send address : NFADDR = dat*/
		nand_reg_p->nfaddr = dat;
	}
}

static int s3c_dev_ready(struct mtd_info *mtd)
{
	return (nand_reg_p->nfstat & (1<<0));
}

static int s3c_nand_init(void)
{
	struct clk *clk;
	/*1.assgin a nand_chip struct*/
	s3c_chip = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);

	nand_reg_p = ioremap(0x4E000000, sizeof(s3c_nand_regs));
	
	/*2.setup*/
	/*2.1setup nand_chip for nand_scan function used,if not know what set , first see nand_scan what used
	*it should provide : select chip \ send command \ send address \ send data \ read data \ judge state of function
	*/
	s3c_chip->select_chip	= s3c_select_chip;
	s3c_chip->cmd_ctrl	= s3c_nand_cmd_ctrl;
	s3c_chip->IO_ADDR_R = &nand_reg_p->nfdata;  //NFDATA of virtual 
	s3c_chip->IO_ADDR_W= &nand_reg_p->nfdata;  //NFDATA of virtual 
	s3c_chip->dev_ready	= s3c_dev_ready;
	s3c_chip->ecc.mode    = NAND_ECC_SOFT;

	/*3.headware correlation of setup : according NAND FLASH manual setup time parameter*/
	/*enable Nand Flash control of clock*/
	clk = clk_get(NULL, "nand");
	clk_enable(clk);
	
	/* HCLK  = 100MHZ 
	* TACLS: 发出CLE/ALE之后多长时间才发出nWE信号，从NandFlash 可知CLE\ALE可以同时发出，所以TACLS=0;
	*TWRPH0 : nWR的脉冲宽度,HCLK * (WTRPH0 + 1) ,从NAND 手册上看出他们要大于等于12ns , 所以TWRPH0>=1;
	*TWRPH1 : nWR变为高电平后多长时间CLE/ALE才能变为低电平，从Nand 手册可知>=5ns,所以TWRPH1 >=0;
	*/
#define 	TACLS 		0
#define	TWRPH0 	1
#define 	TWRPH1		0
	
	nand_reg_p->nfconf = (TACLS << 12) | (TWRPH0 << 8) | (TWRPH1 << 4);
	/*NFCONT
	*bit[1]  -- set is 1,disable chip select 
	*bit[0] -- set is  1,enable NAND Flash Controler
	*/
	nand_reg_p->nfcont |= (1<<1) | (1<<0); 
	
	/*4.used : nand_scan*/
	s3c_mtd = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
	s3c_mtd->priv = s3c_chip;
	s3c_mtd->owner = THIS_MODULE;
	
	nand_scan(s3c_mtd, 1);    /*discern Nand Flash , structure mtd_info struct */
	/*5.add_mtd_partitions*/
	//add_mtd_partitions(s3c_mtd,s3c_nand_part,4);
	mtd_device_register(s3c_mtd, s3c_nand_part, 4);

	//add_mtd_device(s3c_mtd);  //assign a overall
	return 0;
}

static void s3c_nand_exit(void)
{
	mtd_device_unregister(s3c_mtd);
	//del_mtd_partitions(s3c_mtd);
	kfree(s3c_mtd);
	iounmap(nand_reg_p);
	kfree(s3c_chip);
}


module_init(s3c_nand_init);
module_exit(s3c_nand_exit);
MODULE_LICENSE("GPL");

