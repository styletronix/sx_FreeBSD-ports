<?php
/*
 * sxbind_backup.php
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

require_once("config.inc");
require_once("util.inc");
require_once("/usr/local/pkg/sxbind_zoneparser.inc");

if (config_get_path('installedpackages/sxbind/config/0/dynamiczonebackup') == "on") {
    $writeconfig = false;

    foreach (config_get_path('installedpackages/sxbindzone/config', []) as $idx => $zone) {
        if ($zone['type'] == "master") {
            try {
                $zone_string = ZoneParser::get_zone_as_string($zone);

                if ($zone_string) {
                    $zone_string_enc = base64_encode($zone_string);
                    if ($zone['backup'] != $zone_string_enc) {
                        config_set_path("installedpackages/sxbindzone/config/{$idx}/backup",   $zone_string_enc);
                        $writeconfig = true;
                    }
                }
            } catch (Exception $ex) {
                log_error('[sxbind] ERROR Could not backup zone ' . $zone['name' . ': ' - $ex->getMessage()]);
                continue;
            }
        }
    }

    if ($writeconfig) {
        write_config('[sxbind] Backup for master zones created.');
    }
}
?>
