--- config/rules.mk.orig	Sat Jan 25 16:40:16 2003
+++ config/rules.mk	Sat Jan 25 16:42:56 2003
@@ -411,6 +411,12 @@
 endif
 endif
 
+ifeq ($(OS_ARCH),FreeBSD)
+ifdef IS_COMPONENT
+EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
+endif
+endif
+
 ifeq ($(OS_ARCH),NetBSD)
 ifneq (,$(filter arc cobalt hpcmips mipsco newsmips pmax sgimips,$(OS_TEST)))
 ifeq ($(MODULE),layout)
