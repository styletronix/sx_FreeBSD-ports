<?php
/*
 * pkg_sxbind.inc
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2022-2023 Rubicon Communications, LLC (Netgate)
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

global $shortcuts;

$shortcuts['sxbind']				= [];
$shortcuts['sxbind']['main']		= '/pkg_edit.php?xml=sxbind.xml';
$shortcuts['sxbind']['service']	= 'named';

$shortcuts['sxbind_acls']			= $shortcuts['sxbind'];
$shortcuts['sxbind_sync']			= $shortcuts['sxbind'];
$shortcuts['sxbind_views']		= $shortcuts['sxbind'];
$shortcuts['sxbind_zones']		= $shortcuts['sxbind'];
