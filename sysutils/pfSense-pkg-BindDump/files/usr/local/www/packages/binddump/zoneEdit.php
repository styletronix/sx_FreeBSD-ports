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
$chkzone = '/usr/local/sbin/named-checkzone';


if ($_POST) {
    $input_errors = array();
    $savemsg = array();
    $post = $_POST;
    $loadZone = false;

    if ($post['zoneselect'] != $post['current_zone'] || $post["action"] == 'reload') {
        if ($post['zone_editable'] == "true") {
            $input_errors[] = "Zone is in Edit-Mode. End Edit-Mode before switching to another Zone.";
            $post['zoneselect'] = $post['current_zone'];
        } else {
            $loadZone = true;
            $post['zone_editable'] = "false";
            $post['current_zone'] = $post['zoneselect'];
        }
    }

    $selectedZone = explode('__', htmlspecialchars_decode($post['current_zone']));
    $zoneview = $selectedZone[0];
    $zonename = $selectedZone[1];
    $zonename_reverse = $selectedZone[2];
    $zonetype = $selectedZone[3];

    if ($post["action"] == 'save') {
        $tempDB = tempnam("/tmp", "validate_zone");
        file_put_contents($tempDB, $post['zone_data']);

        // validate and save to DB if successfull.
        exec($chkzone . ' -F text ' .
            '-o ' . escapeshellarg(CHROOT_LOCALBASE . "/etc/namedb/{$zonetype}/{$zoneview}/{$zonename}.DB") . ' ' .
            escapeshellarg($zonename_reverse) . ' ' .
            escapeshellarg($tempDB) . ' 2>&1', $output, $resultCode);

        unlink($tempDB);

        if ($resultCode == 0) {
            $thaw = true;
            $loadZone = true;

            $savemsg[] = "Zonedata saved.";
            array_merge($savemsg, $output);
        } else {
            $input_errors[] = "Validation of new Zonefile failed. Code {$resultCode}";
            array_merge($input_errors, $output);
            $loadZone = false;
        }
    }

    if ($post["action"] == 'thaw' || $thaw == true) {
        exec("{$rndc} thaw " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);

        if ($resultCode == 0) {
            $post['zone_editable'] = "false";
            $savemsg[] = "Thaw successfull.";
            array_merge($savemsg, $output);
        } else {
            $input_errors[] = "RNDC THAW throwed an exception. Zone {$zonename_reverse} may still be frozen. Code {$resultCode}";
            array_merge($input_errors, $output);
        }
    }

    if ($post["action"] == 'thawall') {
        exec("{$rndc} thaw" . ' 2>&1', $output, $resultCode);

        if ($resultCode == 0) {
            $post['zone_editable'] = "false";
            $savemsg[] = "End-Edit (Thaw All) successfull.";
            array_merge($savemsg, $output);
        } else {
            $input_errors[] = "RNDC THAW throwed an exception. Code {$resultCode}";
            array_merge($input_errors, $output);
        }
    }

    if ($post["action"] == 'freeze') {
        exec("{$rndc} freeze " . escapeshellarg($zonename_reverse) . " IN " . escapeshellarg($zoneview) . ' 2>&1', $output, $resultCode);

        if ($resultCode <= 1) {
            $loadZone = true;
            $post['zone_editable'] = "true";

            $savemsg[] = "Zone frozen. Don't forget to END EDIT before leaving.";
            array_merge($savemsg, $output);
        } else {
            $input_errors[] = "RNDC Freeze throwed an exception. Code {$resultCode}";
            array_merge($input_errors, $output);
        }
    }

    if ($loadZone === true) {
        try {
            $post['zone_data'] = binddump_compilezone($zoneview, $zonename_reverse);
            $savemsg[] = "Zone Data loaded.";
        } catch (Exception $e) {
            $post['zone_data'] = '';
            $post['zone_editable'] = "false";
            $input_errors[] = $e->getMessage();
        }
    }
}

$pgtitle = array(gettext("Status"), gettext("Edit zone"));
$shortcut_section = "bind";

include("head.inc");

$tab_array = array();
$tab_array[] = array(gettext("Show all Zones"), false, "/packages/binddump/binddump.php");
$tab_array[] = array(gettext("Edit RAW Zone File"), true, "/packages/binddump/zoneEdit.php");
$tab_array[] = array(gettext("Edit Zone File"), false, "/packages/binddump/zoneEditor.php");
display_top_tabs($tab_array);

$zonelist = [];
foreach (binddump_get_zonelist() as $zone) {
    if ($zone['type'] == 'master') {
        $zonelist[$zone['view'] . '__' . $zone['name'] . '__' . binddump_reverse_zonename($zone) . '__' . $zone['type']] = binddump_reverse_zonename($zone) . '  (' . $zone['view'] . ')';
    }
}
ksort($zonelist);

if ($input_errors) {
    print_input_errors($input_errors);
}

if (!empty($savemsg)) {
    print_info_box(implode("\n", $savemsg), 'success');
}

?>

<form class="form-horizontal" method="post" action="zoneEdit.php" enctype="multipart/form-data">
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">
                <?= gettext('Zone') ?>
            </h2>
        </div>
        <div class="panel-body">
            <div class="form-group">
                <label for="zoneselect" class="col-sm-2 control-label">
                    <span>
                        <?= gettext('Zone') ?>
                    </span>
                </label>
                <div class="col-sm-10">
                    <select class="form-control" name="zoneselect" id="zoneselect" onchange="this.form.submit()">
                        <option value="">
                            <?= gettext('Select Zone...') ?>
                        </option>
                        <? foreach ($zonelist as $key => $value) { ?>
                            <option <? if ($key == $post['current_zone']) {
                                    print('selected="selected"');
                                } ?> value="<?= $key ?>"><?= $value ?>
                            </option>
                        <? } ?>
                    </select>
                </div>
            </div>
            <div class="form-group">
                <label class="col-sm-2 control-label">
                    <span>
                        <?= gettext('Actions') ?>
                    </span>
                </label>
                <div class="col-sm-10 btn-group">

                    <button class="btn btn-default" type="submit" value="freeze" name="action">
                        <?= gettext('Start Edit-Mode') ?>
                    </button>
                    <button class="btn btn-success" type="submit" value="save" name="action">
                        <?= gettext('Save changes') ?>
                    </button>
                    <button class="btn btn-default" type="submit" value="thaw" name="action">
                        <?= gettext('End Edit-Mode') ?>
                    </button>
                    <button class="btn btn-default" type="submit" value="thawall" name="action">
                        <?= gettext('End Edit-Mode (All Zones)') ?>
                    </button>
                </div>
            </div>
            <div class="form-group">
                <div class="col-sm-2"></div>
                <div class="col-sm-10 help-block">
                    <?= gettext('While in Edit-Mode, DDNS Updates are disabled to prevent inconsistency.') ?>
                </div>
            </div>
        </div>
    </div>

    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">
                <?= gettext('Zone File') ?>
            </h2>
        </div>
        <div class="panel-body">
            <textarea rows="10" class="col-sm-12 form-control" name="zone_data" id="zone_data" wrap="off" <? if ($post['zone_editable'] !== 'true') {
                print('readonly="readonly"');
            } ?>><?= $post['zone_data'] ?></textarea>
        </div>
    </div>
    <div class="col-sm-10 col-sm-offset-2">
        <button class="btn btn-success" type="submit" value="save" name="action"><?= gettext('Save changes / End Edit-Mode') ?>
        </button>
        </div>
    </div>

    <div id="dlg_updatestatus" class="modal fade" role="dialog" aria-labelledby="dlg_updatestatus" aria-hidden="true">
        <div class="modal-dialog modal-lg">
            <div class="modal-content">
                <div class="modal-header">
                    <button type="button" class="close" data-dismiss="modal" aria-label="Close">
                        <span aria-hidden="true">×</span>
                    </button>
                    <h3 class="modal-title">Aktion</h3>
                </div>
                <div class="modal-body">
                    <div class="form-group">
                        <label for="dlg_updatestatus_text" class="col-sm-2 control-label">
                        </label>
                        <div class="col-sm-8">
                            <textarea rows="10" class="row-fluid col-sm-10" name="dlg_updatestatus_text"
                                id="dlg_updatestatus_text" wrap="off">...Loading...</textarea>
                        </div>
                    </div>
                </div>
                <div class="modal-footer">
                    <input class="btn btn-primary" type="submit" value="close" data-dismiss="modal">
                </div>
            </div>
        </div>
    </div>

    <input type='hidden' value="<?= $post['zone_editable'] ?>" name="zone_editable" />
    <input type='hidden' value="<?= $post['current_zone'] ?>" name="current_zone" />
</form>
<?
include('foot.inc');
?>
<script type="text/javascript">
    function showMessage($text) {
        $('#dlg_updatestatus_text').text($text);
        $('#dlg_updatestatus_text').attr('readonly', true);
        $('#dlg_updatestatus').modal('show');
    }

    function showWait($text) {
        $('#dlg_updatestatus_text').text($text + '<br/><br/>Please wait for the process to complete.<br/><br/>This dialog will auto-close when the update is finished.<br/><br/>' +
            '<i class="content fa fa-spinner fa-pulse fa-lg text-center text-info"></i>');
        $('#dlg_updatestatus_text').attr('readonly', true);
        $('#dlg_updatestatus_text').modal('show');
    }

    function hideWait() {
        $('#dlg_updatestatus_text').modal('hide');
    }

    events.push(function () {
        $(window).on('beforeunload', function () {
            if ($("#zone_editable").val() == "true") {
                event.returnValue = `Discard changes, and leave Page? Remember it is required to end Edit Mode to re-enable DDNS Updates.`;
            };
        });
    });
</script>
