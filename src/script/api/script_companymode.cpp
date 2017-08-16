/* $Id: script_companymode.cpp 26482 2014-04-23 20:13:33Z rubidium $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_companymode.cpp Implementation of ScriptCompanyMode. */

#include "../../stdafx.h"
#include "script_companymode.hpp"

#include "../../safeguards.h"

ScriptCompanyMode::ScriptCompanyMode(int company)
{
	if (company < OWNER_BEGIN || company >= MAX_COMPANIES) company = INVALID_COMPANY;

	this->last_company = ScriptObject::GetCompany();
	ScriptObject::SetCompany((CompanyID)company);
}

ScriptCompanyMode::~ScriptCompanyMode()
{
	ScriptObject::SetCompany(this->last_company);
}
