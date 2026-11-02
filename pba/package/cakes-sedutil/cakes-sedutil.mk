################################################################################
#
# cakes-sedutil
#
################################################################################

# Built from the repository this external tree lives in
CAKES_SEDUTIL_VERSION = $(shell cat $(BR2_EXTERNAL_CAKES_PBA_PATH)/../VERSION)
CAKES_SEDUTIL_SITE = $(BR2_EXTERNAL_CAKES_PBA_PATH)/..
CAKES_SEDUTIL_SITE_METHOD = local
CAKES_SEDUTIL_LICENSE = GPL-3.0+
CAKES_SEDUTIL_LICENSE_FILES = Common/LICENSE.txt
CAKES_SEDUTIL_AUTORECONF = YES
# old upstream code, don't let new compiler warnings break the image
CAKES_SEDUTIL_MAKE_OPTS = WERROR=
CAKES_SEDUTIL_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = \
	--exclude /pba --exclude /images --exclude /windows --exclude /output \
	--exclude /branding --exclude /tests --exclude /.github --exclude /dist \
	--exclude /qemu-smoke

# scripts/build.sh exports CAKES_PBA_VERSION from git describe
CAKES_SEDUTIL_GIT_VERSION = $(or $(CAKES_PBA_VERSION),$(CAKES_SEDUTIL_VERSION))

define CAKES_SEDUTIL_SET_VERSION
	echo '#define GIT_VERSION "$(CAKES_SEDUTIL_GIT_VERSION)"' > $(@D)/linux/Version.h
endef
CAKES_SEDUTIL_POST_CONFIGURE_HOOKS += CAKES_SEDUTIL_SET_VERSION

$(eval $(autotools-package))
