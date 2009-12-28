## Process this file with automake to produce Makefile.in

###########   libcot documentation   ###########
## archive with HTML documentation
libcothtml = libcot-html-$(LIBCOT_VERSION_DOT).tar.gz

## PDF file
libcotpdf = libcot_manual_$(LIBCOT_VERSION_DOT).pdf

###########   full project documentation   ###########
## archive with HTML documentation
fullhtml = full-cot-html-$(PACKAGE_VERSION).tar.gz			 

## PDF file
fullpdf = full_reference_manual_$(PACKAGE_VERSION).pdf

doc_DATA =  $(top_builddir)/doc/$(libcothtml) \
			$(top_builddir)/doc/$(libcotpdf) \
			$(top_builddir)/doc/$(fullhtml) \
			$(top_builddir)/doc/$(fullpdf)

libcot.doxygen.stamp: libcot.doxyfile
	$(DOXYGEN) $(DOXYFLAGS) $<
	echo Timestamp > $@

$(top_builddir)/doc/$(libcothtml): libcot.doxygen.stamp
	tar chof - libcot/html | gzip -9 -c >$@	 			
	
$(top_builddir)/doc/$(libcotpdf): libcot.doxygen.stamp
	cd libcot/latex && $(MAKE) $(AM_MAKEFLAGS) all || exit 1
	cp libcot/latex/refman.pdf $@

full.doxygen.stamp: full.doxyfile
	$(DOXYGEN) $(DOXYFLAGS) $<
	echo Timestamp > $@
		
$(top_builddir)/doc/$(fullhtml): full.doxygen.stamp
	tar chof - full/html | gzip -9 -c >$@
	
$(top_builddir)/doc/$(fullpdf): full.doxygen.stamp
	cd full/latex && $(MAKE) $(AM_MAKEFLAGS) all || exit 1
	cp full/latex/refman.pdf $@	

CLEANFILES = libcot.warn.txt libcot.doxygen.stamp \
			 full.warn.txt full.doxygen.stamp \
			 $(top_builddir)/doc/$(libcothtml) \
			 $(top_builddir)/doc/$(libcotpdf) \
			 $(top_builddir)/doc/$(fullhtml) \
			 $(top_builddir)/doc/$(fullpdf)

clean-local:
	-rm -rf libcot
	-rm -rf full
