<?php
/*
 * sxbind_keys.php
 *
 * Copyright (c) 2023 Andreas W. Pross (Styletronix.net)
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use sxbind\ZoneParser;

require_once("guiconfig.inc");
require_once("config.inc");
require_once("/usr/local/pkg/sxbind_zoneparser.inc");
require_once("/usr/local/pkg/sxbind.inc");

if ($_POST) {
    if ($_POST['action'] == 'create_tsig') {
        $name = '"' . htmlspecialchars_decode($_POST['tsig_name']) . '"';
        $key = ZoneParser::create_tsig_key($name);
        print($key);
        exit;
    }

    if ($_POST['action'] == 'delete_tsig') {
        $id = str_replace(' ', '_', htmlspecialchars_decode($_POST['tsig_name']));
        config_del_path('installedpackages/sxbind/tsig_keys/' . $id);
        if (write_config(gettext("TSIG Key removed."))) {
            sxbind_sync();
        }
        exit;
    }

    if ($_POST['action'] == 'tsig_to_bind_config') {
        $id = str_replace(' ', '_', htmlspecialchars_decode($_POST['tsig_name']));
        $name = '"' . htmlspecialchars_decode($_POST['tsig_name']) . '"';
        $newkey = [];
        $newkey['name'] = $name;
        $newkey['key'] = base64_encode($_POST['tsig_key']);

        config_set_path('installedpackages/sxbind/tsig_keys/' . $id, $newkey);

        if (write_config(gettext("TSIG Key added."))) {
            sxbind_sync();
        }
    }

    if ($_POST['action'] == 'convert_ip_to_ptr') {
        try {
            print(ZoneParser::ip_to_ptr($_POST['ip']));
            exit;
        } catch (Exception $ex) {
            die(500);
        }

    }
}

$pgtitle = array(gettext("Services"), gettext("Bind Tools"));
$shortcut_section = "sxbind";
include("head.inc");

get_top_tabs();

if ($input_errors) {
    print_input_errors($input_errors);
}

if (!empty($savemsg)) {
    print_info_box(implode("\n", $savemsg), 'success');
}

?>

<form class="form-horizontal" method="post" enctype="multipart/form-data">
    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">
                <?= gettext('TSIG') ?>
            </h2>
        </div>
        <div class="panel-body">
            <div class="form-group">
                <label for="tsig_name" class="col-sm-2 control-label">
                    <span>
                        <?= gettext('Key Name') ?>
                    </span>
                </label>
                <div class="col-sm-10">
                    <input type="text" class="form-control" name="tsig_name" id="tsig_name"
                        value="<?= $_POST['tsig_name'] ?>" />
                </div>
            </div>
            <div id="tsig_key_field" style="display: none" class="form-group">
                <label for="tsig_key" class="col-sm-2 control-label">
                    <span>
                        <?= gettext('TSIG-Key') ?>
                    </span>
                </label>
                <div class="col-sm-10">
                    <textarea class="form-control" id="tsig_key" name="tsig_key" rows="5" readonly="readonly"
                        val="Click on  " \Create Key\" to show new TSIG-Key "></textarea>
                </div>
            </div>
        </div>

        <div class=" panel-footer">
            <button class="btn btn-default" id="btn_create_tsig" name="action" value="create_tsig">
                <?= gettext("Create Key") ?>
            </button>
             <button class="btn btn-default" id="btn_tsig_to_bind_config" name="action" value="tsig_to_bind_config">
                <?= gettext("Add key to BIND Config") ?>
            </button>
        </div>
    </div>


    <div class="panel panel-default">
        <div class="panel-heading">
            <h2 class="panel-title">
                <?= gettext('TSIG-Keys') ?>
            </h2>
        </div>
        <div id="tsigkeys" class="table-responsive panel-body">
            <table class="table table-hover table-striped table-condensed">
                <thead>
                    <tr>
                        <th width="30%">Name</th>
                        <th width="60%">Key</th>
                        <th>Actions</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>
                            rndc-key
                        </td>
                        <td>
                            <pre>[Internal Key for control communication]</pre>
                        </td>
                    </tr>
<?php
foreach (config_get_path('installedpackages/sxbind/tsig_keys/', []) as $key => $value) {
    if (is_array($value)) {
        $key_string = base64_decode($value['key'][0]);
    } else {
        $key_string = "[Error]";
    }
    ?>
                                                        <tr data-id="<?= $key; ?>">
                                                            <td>
                                                                <?= $key; ?>
                                                            </td>
                                                            <td>
                                                                <pre><?= $key_string ?></pre>
                                                            </td>
                                                            <td>
                                                                <button name="action" type="submit" class="btn btn-danger btn-sm" value="delete_tsig_key" title="Delete selected key">
                                                                    <i class="fa fa-trash icon-embed-btn"></i><?= gettext('Delete') ?>
                                                                </button>
                                                            </td>
                                                        </tr>
                                                        <?php
}
?>
                </tbody>
            </table>
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
</form>

<?
include('foot.inc');
?>


<script type="text/javascript">
    function showMessage($text) {
        $('#dlg_updatestatus_text').text($text)
        $('#dlg_updatestatus_text').attr('readonly', true)
        $('#dlg_updatestatus').modal('show')
    }

    function showWait($text) {
        $('#dlg_updatestatus_text').text($text + '<br/><br/>Please wait for the process to complete.<br/><br/>This dialog will auto-close when the update is finished.<br/><br/>' +
            '<i class="content fa fa-spinner fa-pulse fa-lg text-center text-info"></i>')
        $('#dlg_updatestatus_text').attr('readonly', true)
        $('#dlg_updatestatus_text').modal('show')
    }

    function hideWait() {
        $('#dlg_updatestatus_text').modal('hide')
    }

    function create_tsig(addToConfig) {
        return new Promise((resolve, reject) => {
            if ($("#tsig_name").val() == "") {
                showMessage('A name is required for TSIG Key');
                reject('A name is required for TSIG Key');
                return;
            }

            $.ajax(
                {
                    type: 'post',
                    data: {
                        action: "create_tsig",
                        tsig_name: $("#tsig_name").val(),
                        tsig_to_bind_config: addToConfig
                    },
                    success: function (data) {
                        $("#tsig_key").val(data)
                        $("#tsig_key_field").show()
                        resolve(data);
                    },
                    error: function (data) {
                        showMessage(data.responseText);
                        reject(data.responseText);
                    }
                });
        });
    }

    function delete_tsig_key(tsigname) {
        return new Promise((resolve, reject) => {
            $.ajax(
                {
                    type: 'post',
                    data: {
                        action: "delete_tsig",
                        tsig_name: tsigname
                    },
                    success: function (data) {
                        resolve(data)
                    },
                    error: function (data) {
                        showMessage(data.responseText)
                        reject(data.responseText)
                    }
                })
        })

    }

    events.push(function () {
        $("[value=\"delete_tsig_key\"").on("click", function (e) {
            e.preventDefault()
            var $btn = $(this)
            var row = $btn.closest("tr")
            var tsigname = row.attr("data-id")

            $btn.button('loading')
            delete_tsig_key(tsigname)
                .then(() => {
                    row.remove()
                })
                .finally(() => {
                    $btn.button('reset')
                })
        })

        $("#btn_create_tsig").on("click", function (e) {
            e.preventDefault()
            var $btn = $(this)
            $btn.button('loading')

            create_tsig().finally(() => {
                $btn.button('reset')
            })
        })

        $("#btn_create_tsig_add").on("click", function (e) {
            e.preventDefault()
            var $btn = $(this)
            $btn.button('loading')

            create_tsig(true).finally(() => {
                $btn.button('reset')
            })
        })
    })
</script>
