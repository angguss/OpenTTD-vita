/* $Id: script_bridgelist.cpp 26482 2014-04-23 20:13:33Z rubidium $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_bridgelist.cpp Implementation of ScriptBridgeList and friends. */

#include "../../stdafx.h"
#include "script_bridgelist.hpp"
#include "script_bridge.hpp"
#include "../../bridge.h"

#include "../../safeguards.h"

ScriptBridgeList::ScriptBridgeList()
{
	for (byte j = 0; j < MAX_BRIDGES; j++) {
		if (ScriptBridge::IsValidBridge(j)) this->AddItem(j);
	}
}

ScriptBridgeList_Length::ScriptBridgeList_Length(uint length)
{
	for (byte j = 0; j < MAX_BRIDGES; j++) {
		if (ScriptBridge::IsValidBridge(j)) {
			if (length >= (uint)ScriptBridge::GetMinLength(j) && length <= (uint)ScriptBridge::GetMaxLength(j)) this->AddItem(j);
		}
	}
}