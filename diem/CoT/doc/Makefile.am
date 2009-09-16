## Process this file with automake to produce Makefile.in

###########   libcot documentation   ###########
## archive with HTML documentation
libcothtml = libcot-html-$(PACKAGE_VERSION).tar.gz

## PDF file
libcotpdf = libcot_manual.pdf

###########   full project documentation   ###########
## archive with HTML documentation
fullhtml = full-cot-html-$(PACKAGE_VERSION).tar.gz			 

## PDF file
fullpdf = full_reference_manual.pdf

###########        other documents         ###########
dist_doc_DATA = long_polling.pdf

if HAVE_DOXYGEN
 ## Build fresh documentation by doxygen
 SUBDIRS = doxygen/
else
 ## Use ready documentation files
 dist_doc_DATA += $(libcothtml) $(libcotpdf) $(fullhtml) $(fullpdf)   
 ## $(fullhtml) $(fullpdf) 
endif



