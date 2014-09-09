
MPICC=mpicc
MPICC_OPTS=-g
# do not forget "module load libccc_user" before
ifneq ($(CCC_LIBCCC_USER_LDFLAGS),)
LIBS=$(CCC_LIBCCC_USER_LDFLAGS) 
MPICC_OPTS+=-D __HAVE_LIBCCC_USER__ -I$(CCC_LIBCCC_USER_INC_DIR)
endif

.PHONY: all
all : glost_launch

%:%.c
	$(MPICC) $(MPICC_OPTS) -o $@ $< $(LIBS)

clean:
	$(RM) glost_launch






