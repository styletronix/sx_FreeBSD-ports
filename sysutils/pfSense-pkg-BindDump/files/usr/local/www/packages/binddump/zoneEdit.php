<?php
/*
 * binddump.php
 */
require_once("guiconfig.inc");
require_once("config.inc");
require_once("/usr/local/pkg/binddump.inc");

define('BIND_LOCALBASE', '/usr/local');
define('CHROOT_LOCALBASE', '/var/etc/named');
$rndc_conf_path = BIND_LOCALBASE . "/etc/rndc.conf";
$rndc = "/usr/local/sbin/rndc -q -c {$rndc_conf_path}";

if ($_POST) {
    $input_errors = array();
    $post = $_POST;
    
    if (!empty($_POST["thawall"])) {
        exec("{$rndc} thaw" . ' 2>&1', $output, $resultCode);
        if ($resultCode !== 0) {
            $input_errors[] = "RNDC THAW throwed an exception. Code {$resultCode} \n " . implode("\n", $output);
        } else {
            $post['zone_editable'] = "false";
            $savemsg = "Thaw successfull.\n\n" . implode("\n", $output);
        }
    }

    if ($_POST['zoneselect'] !== $post['current_zone'] || !empty($_POST["reload"])) {
        if ($_POST['zone_editable'] == "true") {
            $input_errors[] = "Zone is in Edit-Mode. End Edit-Mode before switching to another Zone.";
            $post['zoneselect'] = $post['current_zone'];
        } else {
            try {
                $selectedZone = explode('__', htmlspecialchars_decode($_POST['zoneselect']));
                $post['zone_data'] = binddump_compilezone($selectedZone[0], $selectedZone[1]);
            } catch (Exception $e) {
                $post['zone_data'] = '';
                $input_errors[] = 'Exception: ' . $e->getMessage();
            }
            $post['zone_editable'] = "false";
            $post['current_zone'] = $_POST['zoneselect'];
        }
    }

    $selectedZone = explode('__', htmlspecialchars_decode($post['current_zone']));
    $zoneview = $selectedZone[0];
    $zonename = $selectedZone[1];
    $zonename_reverse = $selectedZone[2];
    $zonetype = $selectedZone[3];

    if (!empty($_POST["save"])) {
        $tempDB = tempnam("/tmp", "validate_zone");
        file_put_contents($tempDB, $post['zone_data']);

        // validate and save to DB if successfull.
        exec('/usr/local/sbin/named-checkzone -F text ' .
            '-o ' . escapeshellarg(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB") . ' ' .
            escapeshellarg($zonename_reverse) . ' ' .
            escapeshellarg($tempDB) . ' 2>&1', $output, $resultCode);

        unlink($tempDB);

        if ($resultCode !== 0) {
            $input_errors[] = "named-checkzone throwed an exception. Code {$resultCode} \n " . implode("\n", $output);
        } else {
            $savemsg = implode("\n", $output);
            exec("{$rndc} thaw " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);
            $post['zone_editable'] = "false";
        }
    }

    if (!empty($_POST["thaw"])) {
        exec("{$rndc} thaw " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);
        if ($resultCode !== 0) {
            $input_errors[] = "RNDC THAW throwed an exception. Zone {$zonename_reverse} may still be frozen. Code {$resultCode} \n " . implode("\n", $output);
        } else {
            $post['zone_editable'] = "false";
            $savemsg = "Thaw successfull.\n\n" . implode("\n", $output);
        }
    }

    

    if (!empty($_POST["freeze"])) {
        exec("{$rndc} freeze " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);

        if ($resultCode !== 0) {
            $input_errors[] = "named-checkzone throwed an exception. Code {$resultCode} \n " . implode("\n", $output);
        } else {
            $post['zone_editable'] = "true";
            $post['zone_data'] = binddump_compilezone($zoneview, $zonename);
            $savemsg = implode("\n", $output) . "\n\n Zone frozen and file reloaded.\n Don't forget to thaw zone before leaving.";
        }
    }
}

$pgtitle = array(gettext("Status"), gettext("Edit zone"));
$shortcut_section = "bind";

include("head.inc");

$tab_array = array();
$tab_array[] = array(gettext("Database"), false, "/packages/binddump/binddump.php");
$tab_array[] = array(gettext("Edit RAW Zone File"), true, "/packages/binddump/zoneEdit.php");
display_top_tabs($tab_array);

$zonelist = [];
foreach (binddump_get_zonelist() as $zone) {
    $zonelist[$zone['view'] . '__' . $zone['name'] . '__' . binddump_reverse_zonename($zone) . '__' . $zone['type']] = $zone['view'] . ': ' . binddump_reverse_zonename($zone);
}

if ($input_errors) {
    print_input_errors($input_errors);
}

if ($savemsg) {
    print_info_box($savemsg, 'success');
}

$form = new Form();
$form->setMultipartEncoding();

/* #region section ZONE */
$section = new Form_Section('Zone');

$zoneselect = new Form_Select('zoneselect', 'Zone', $post['zoneselect'], $zonelist, false);
$zoneselect->setOnchange("this.form.submit();");
$section->addInput($zoneselect);

$group = new Form_Group('Action');

$btnreload = new Form_Button('load', 'Reload Zone');
$group->add($btnreload);

$btnfreeze = new Form_Button('freeze', 'Start Edit');
$btnfreeze->setHelp('While in Edit-Mode, DDNS Updates are disabled.');
$group->add($btnfreeze);

$btnthaw = new Form_Button('thaw', 'End Edit');
$group->add($btnthaw);

$btnthawall = new Form_Button('thawall', 'End Edit - ALL ZONES');
$group->add($btnthawall);

$section->add($group);
$form->add($section);
/* #endregion */

/* #region section Edit Zone */
$section = new Form_Section('Edit Zone');
$zonetext = new Form_Textarea(
    'zone_data',
    'Zone File',
    $post['zone_data']
);
$zonetext->setWidth(8);
$zonetext->setAttribute("rows", "25");
$zonetext->setAttribute("wrap", "off");
if ($post['zone_editable'] !== "true") {
    $zonetext->setReadonly();
}
$section->addInput($zonetext);

$form->add($section);
/* #endregion */

$form->addGlobal(new Form_Input('current_zone', null, 'hidden', $post['current_zone']));
$form->addGlobal(new Form_Input('zone_editable', null, 'hidden', $post['zone_editable']));

/* #region Create a Modal Dialog */
$modal = new Modal('Aktion', 'dlg_updatestatus', 'large', 'Close');
$modal->addInput(
    new Form_Textarea(
        'dlg_updatestatus_text',
        '',
        '...Loading...'
    )
)->removeClass('form-control')->addClass('row-fluid col-sm-10')->setAttribute('rows', '10')->setAttribute('wrap', 'off');
$form->add($modal);

$modal = new Modal(gettext('Please wait'), 'dlg_wait', false, 'Close');
$modal->addInput(
    new Form_StaticText(
        'dlg_wait_text',
        'Please wait for the process to complete.<br/><br/>This dialog will auto-close when the update is finished.<br/><br/>' .
        '<i class="content fa fa-spinner fa-pulse fa-lg text-center text-info"></i>'
    )
);
$form->add($modal);
/* #endregion */

print $form;

?>
<script type="text/javascript">
    events.push(function () {
        $(window).on('beforeunload', function () {
            $.ajax({
                url: 'zoneEdit.php',
                type: 'POST',
                data: {
                    'thawall': 'thawall'
                }
            });
        });
    });
</script>
<?
include('foot.inc');
?>
