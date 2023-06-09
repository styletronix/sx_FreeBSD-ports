<?php

function get_top_tabs()
{
    $tab_array = array();
    $tab_array[] = array(gettext("Settings"), false, "/pkg_edit.php?xml=sxbind.xml");
    $tab_array[] = array(gettext("ACLs"), false, "/pkg_edit.php?xml=sxbind_acls.xml");
    $tab_array[] = array(gettext("Views"), false, "/pkg_edit.php?xml=sxbind_views.xml");
    $tab_array[] = array(gettext("Zones"), false, "/pkg_edit.php?xml=sxbind_zones.xml");
    $tab_array[] = array(gettext("Tools"), false, "/sxbind_tools.php");
    $tab_array[] = array(gettext("Show all Zone Records"), false, "/sxbind_dump.php");
    $tab_array[] = array(gettext("Edit Zone DB"), true, "/sxbind_zoneEdit.php");
    $tab_array[] = array(gettext("Sync"), false, "/pkg_edit.php?xml=sxbind_sync.xml");
    display_top_tabs($tab_array);
}
?>
