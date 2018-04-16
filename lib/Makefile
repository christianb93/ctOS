SUBDIRS = internal os linux std 

all: $(SUBDIRS)  

$(SUBDIRS):
	$(MAKE) -C $@
 
.PHONY:  $(SUBDIRS)	
			
clean:
	for i in $(SUBDIRS); do (cd $$i ; $(MAKE) clean) ; done

include Makefile.dep


	
dep:
	for i in $(SUBDIRS); do (cd $$i ; make dep) ; done
	