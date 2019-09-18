###############################################################################
#
# Top level makefile. Forwards $(MAKE) calls to $(SUBDIRS) subdirectories.
#
# Author: 	Alif Ahmed
# email: 	alifahmed@virginia.edu
# Updated: 	Aug 06, 2019
#
###############################################################################

SUBDIRS = cpu gpu

include makedir.mk


