#
# Copyright (c) 2024 FKMG Circuits
#

zephyr_library_amend()
zephyr_library_sources_ifdef(CONFIG_DAC7X78 dac7x78.c)
