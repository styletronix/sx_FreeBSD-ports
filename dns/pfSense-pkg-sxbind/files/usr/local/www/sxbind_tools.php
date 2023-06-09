<?php
/*
 * sxbind_tools.php
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
require_once("sxbind.inc");

if ($_POST) {
    if ($_POST['action'] == 'create_tsig') {
        $key = ZoneParser::create_tsig_key($_POST['tsig_name']);
        print($key);

        if ($_POST['tsig_to_bind_config'] == true) {
            //TODO: Add to config
        }

        exit;
    }

    if ($_POST['action'] == 'convert_ip_to_ptr') {
        try{
            print(ZoneParser::ip_to_ptr($_POST['ip']));
            exit;
        }catch(Exception $ex){
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
                    <input type="text" class="form-control" name="tsig_name" id="tsig_name" />
                </div>
            </div>
            <div class="form-group">
                <label for="tsig_key" class="col-sm-2 control-label">
                    <span>
                        <?= gettext('TSIG-Key') ?>
                    </span>
                </label>
                <div style="display:hidden" id="tsig_key_field" class="col-sm-10">
                    <textarea class="form-control" id="tsig_key" name="tsig_key" rows="5"
                        readonly="readonly" val="Click on  "\Create Key\" to show new TSIG-Key "></textarea>
                </div>
            </div>
        </div>

        <div class="panel-footer">
            <button class="btn btn-default" id="btn_create_tsig" name="action" value="create_tsig">
                <?= gettext("Create Key") ?>
            </button>
             <button class="btn btn-default" id="btn_create_tsig_add" name="action" value="create_tsig">
                <?= gettext("Create Key and add to BIND Config") ?>
            </button>
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
        $("#tsig_key").val("Creating Key....")
        $("#tsig_key_field").show()

        $.ajax(
            {
                type: 'post',
                data: {
                    action: "create_tsig",
                    tsig_name: $("#tsig_name").val(),
                    tsig_to_bind_config: addToConfig
                },
                success: function (data) {
                    hideWait();
                    $("#tsig_key").val(data)
                },
                error: function (data) {
                    hideWait();
                    showMessage(data.responseText);
                }
            });
    }

    events.push(function () {
        $("#btn_create_tsig").on("click", function (e) {
            e.preventDefault()
            create_tsig()
        })

        $("#btn_create_tsig_add").on("click", function (e) {
            e.preventDefault()
            create_tsig(true)
        })
    })
</script>
