# $FreeBSD$

PORTNAME=	pfSense-pkg-sxbind
PORTVERSION=	1.1
PORTREVISION= 2
CATEGORIES=	dns
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

RUN_DEPENDS=	bind916>=9.16.12:dns/bind916
CONFLICTS_INSTALL= pfSense-pkg-bind

MAINTAINER=	andreas.pross@styletronix.net
COMMENT=	BIND DNS suite with updated DNSSEC and DNS64 originally forked from pfSense-pkg-bind

LICENSE=	APACHE20

NO_BUILD=	yes
NO_MTREE=	yes

SUB_FILES=	pkg-install pkg-deinstall
SUB_LIST=	PORTNAME=${PORTNAME}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/pkg
	${MKDIR} ${STAGEDIR}/etc/inc/priv
	${MKDIR} ${STAGEDIR}${DATADIR}
	${MKDIR} ${STAGEDIR}${PREFIX}/www/shortcuts
	${MKDIR} ${STAGEDIR}${PREFIX}/www/widgets/widgets

	
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/sxbind_acls.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/sxbind_sync.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/sxbind_views.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/sxbind_zoneparser.inc \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/sxbind_zones.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/sxbind.inc \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/sxbind.xml \
		${STAGEDIR}${PREFIX}/pkg

	${INSTALL_DATA} ${FILESDIR}/etc/inc/priv/sxbind.priv.inc \
		${STAGEDIR}/etc/inc/priv

	${INSTALL_DATA} ${FILESDIR}${DATADIR}/info.xml \
		${STAGEDIR}${DATADIR}

	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/shortcuts/pkg_sxbind.inc \
		${STAGEDIR}${PREFIX}/www/shortcuts

	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/widgets/widgets/sxbind.widget.php \
		${STAGEDIR}${PREFIX}/www/widgets/widgets

	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/sxbind_dump.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/sxbind_tools.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/sxbind_zoneEdit.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/sxbind.inc \
		${STAGEDIR}${PREFIX}/www

	@${REINPLACE_CMD} -i '' -e "s|%%PKGVERSION%%|${PKGVERSION}|" \
		${STAGEDIR}${DATADIR}/info.xml \
		${STAGEDIR}${PREFIX}/pkg/sxbind.xml \
		${STAGEDIR}${PREFIX}/pkg/sxbind_acls.xml \
		${STAGEDIR}${PREFIX}/pkg/sxbind_sync.xml \
		${STAGEDIR}${PREFIX}/pkg/sxbind_views.xml \
		${STAGEDIR}${PREFIX}/pkg/sxbind_zones.xml \

.include <bsd.port.mk>
