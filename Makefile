MPICC=mpicc
MPICC_OPTS=-g

# do not forget "module load libccc_user" before
ifneq ($(CCC_LIBCCC_USER_LDFLAGS),)
LIBS=$(CCC_LIBCCC_USER_LDFLAGS) 
MPICC_OPTS+=-D __HAVE_LIBCCC_USER__ -I$(CCC_LIBCCC_USER_INC_DIR)
endif

BLEN=134217728 #128MB
#BLEN=67108864 # 64MB
#BLEN=33554432 #32MB
#BLEN=16777216 #16MB
#BLEN=8388608  # 8MB
#CFLAGS=-DDEFAULT_BUFFLEN=$(BLEN) -DDO_NOT_WRITE=1
glost_bcast: MPICC_OPTS+=-std=c99 -D_GNU_SOURCE -DDEFAULT_BUFFLEN=$(BLEN)

.PHONY: all
all : glost_launch glost_bcast

%:%.c
	mpicc -std=c99 -g $(CFLAGS) -o $@ $<

%:%.c
	$(MPICC) $(MPICC_OPTS) -o $@ $< $(LIBS)

clean:
	$(RM) glost_launch glost_bcast






