## Process this file with automake to produce Makefile.in

###########   libcot documentation files   ###########
## archive with HTML documentation
libcothtml = libcot-html-$(PACKAGE_VERSION).tar.gz

## PDF file
libcotpdf = libcot_manual.pdf

if HAVE_DOXYGEN
 ## Build fresh documentation by doxygen
 SUBDIRS = doxygen/
else
 ## Use ready documentation files
 doc_DATA = $(libcothtml) $(libcotpdf)
endif



