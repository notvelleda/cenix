.if !defined(__OUT_OF_SOURCE)
__OUT_OF_SOURCE = yeag # :3

CWD != pwd
PROJECT_ROOT != echo ${CWD}/${PROJECT_ROOT}
TARGET != [ "${BINARY}" ] && echo "${BINARY}" || echo "${LIBRARY}"

# https://www.reddit.com/r/BSD/comments/1gjnqe5/comment/lwtoh41/

MAKEOBJDIR ?= $(PROJECT_ROOT)/build/$(TARGET)

.if ${.OBJDIR} != ${MAKEOBJDIR}
.if !exists(${MAKEOBJDIR})
__mkobjdir != mkdir -p ${MAKEOBJDIR}
__mkobjdir2 != cd "${MAKEOBJDIR}" && echo "${OBJECTS}" | xargs -n 1 dirname | xargs -n 1 mkdir -p
.endif
.OBJDIR: ${MAKEOBJDIR}
.endif

.endif
