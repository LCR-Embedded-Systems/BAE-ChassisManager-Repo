# first load all the images from nand
nand read ${kernel_loadaddr} nand-kernel ${kernel_size}
nand read ${devicetree_loadaddr} nand-devicetree ${devicetree_size}

# setup nand bootargs
run nand_args

# append debug flags to bootargs
setenv bootargs "${bootargs} ignore_loglevel loglevel=8 printk.time=1 earlycon initcall_debug clk_ignore_unused"

bootz ${kernel_loadaddr} - ${devicetree_loadaddr}
