BUILDDIR = build
TARGETDIR = $(BUILDDIR)/target
APP = $(TARGETDIR)/sota_client

default: $(APP)

$(BUILDDIR):
	mkdir $(BUILDDIR)

$(BUILDDIR)/Makefile: $(BUILDDIR)
	cd $(BUILDDIR) && cmake ..

$(APP): $(BUILDDIR)/Makefile
	make -C $(BUILDDIR)

test: $(APP)
	cd $(BUILDDIR) && make test

clean:
	rm -rf build

.PHONY: clean
