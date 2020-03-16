
subdirs = lib test samples

all: $(subdirs)

.PHONY: $(subdirs)
$(subdirs):
	@if [ -d $@ ]; then				\
		$(MAKE) -C $@ || exit 1;	 	\
	fi

check:  $(subdirs)
	$(MAKE) -C test $@

clean:
	@for dir in $(subdirs); do 			\
		if [ -d $$dir ]; then			\
			$(MAKE) -C $$dir $@ || exit 1;	\
		fi					\
	done

