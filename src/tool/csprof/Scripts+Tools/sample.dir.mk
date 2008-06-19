#
# include a makefile like this to add to a global sources directory
#  see Oreilly make book

local_src := $(wildcard $(local_dir)/*.c)
sources += $(local_src)
