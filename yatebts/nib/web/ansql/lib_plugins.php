<?php
/* 
 * lib_plugins.php
 * This file is part of the Yate-LMI Project http://www.yatebts.com
 *
 * Copyright (C) 2016 Null Team
 *
 * This software is distributed under multiple licenses;
 * see the COPYING file in the main directory for licensing
 * information for this specific distribution.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * Includes plugins located in plugins/ directory 
 * Function should be called between including defaults.php & config.php, because some plugins might modify global variables from defaults.php
 */
function include_plugins()
{
	if (!is_dir("plugins"))
		return;
	
	$handle = opendir("plugins");

	while (false !== ($file = readdir($handle))) {
		if (substr($file,-4) != '.php')
			continue;
		else {
			$plugin_name = explode("_", $file);
			require_once("plugins/".$file);
			//loaded_plugin($plugin_name[0]);
		}
	}
}

/**
 * Register hook $hook_handler to be called for hook name $hook_name. If unicity tags are specified then system will return an error in case 
 * @global type $plugin_hooks
 * @param String $hook_name
 * @param Callback $hook_handler
 * @param String $plugin_name
 * @param Array $unicity_tags
 */
function register_hook($hook_name, $hook_handler, $plugin_name=null, $unicity_tags=array())
{
	global $plugin_hooks;
	global $plugin_errors;
	
	if (!isset($plugin_hooks))
		$plugin_hooks = array();
	
	if (!array_key_exists($hook_name, $plugin_hooks))
		$plugin_hooks[$hook_name] = array(
		    "unique_tags" => array(),
		    "hooks"       => array()
		);
	
	foreach ($unicity_tags as $tag) {
		if (isset($plugin_hooks[$hook_name]["unique_tags"][$tag]))
			$plugin_errors[] = "Invalid plugin combination '$plugin_name' - '".$plugin_hooks[$hook_name]["unique_tags"][$tag]."' ($tag). Problems registering hook '$hook_handler' on '$hook_name' for pluggin '$plugin_name'.";
	}
	
	if (!is_callable($hook_handler)) {
		$plugin_errors[] = "Hook handler is not callable: $hook_handler.";
		return;
	}
	
	$plugin_hooks[$hook_name]["hooks"][] = $hook_handler;
}

/**
 * Call registered hooks for specific hook name
 * @global type $plugin_hooks
 * @param type $hook_name
 */
function call_hooks($hook_name)
{
	global $plugin_hooks;
	
	if (isset($plugin_hooks[$hook_name]["hooks"])) {
		foreach ($plugin_hooks[$hook_name]["hooks"] as $cb_hook) {
			if (is_callable($cb_hook))
				call_user_func($cb_hook);
			else {
				Debug::trigger_report("critical", "Tried to call hook '$cb_hook', but it's not callable.");
			}
		}
	}
}

