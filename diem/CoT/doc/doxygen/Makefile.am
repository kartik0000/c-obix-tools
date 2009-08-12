## Process this file with automake to produce Makefile.in

###########   libcot documentation   ###########
## archive with HTML documentation
libcothtml = libcot-html-$(PACKAGE_VERSION).tar.gz

## PDF file
libcotpdf = libcot_manual.pdf

dist_doc_DATA =  $(top_builddir)/doc/$(libcothtml) \
						$(top_builddir)/doc/$(libcotpdf)

$(top_builddir)/doc/$(libcothtml): doxygen.stamp
	tar chof - libcot/html | gzip -9 -c >$@		

doxygen.stamp: libcot.doxyfile
	$(DOXYGEN) $(DOXYFLAGS) $<
	echo Timestamp > $@
	
$(top_builddir)/doc/$(libcotpdf): doxygen.stamp
	cd libcot/latex && $(MAKE) $(AM_MAKEFLAGS) all || exit 1
	cp libcot/latex/refman.pdf $@

CLEANFILES = doxywarn.txt doxygen.stamp \
			 $(top_builddir)/doc/$(libcothtml) \
			 $(top_builddir)/doc/$(libcotpdf)

clean-local:
	-rm -rf libcot
