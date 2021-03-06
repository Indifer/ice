# **********************************************************************
#
# Copyright (c) 2003-2016 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

$(project)_type			= xcode
$(project)_platforms		= iphonesimulator iphoneos
$(project)_configs		= xcodesdk arc-xcodesdk
$(project)_dependencies		= IceObjC IceSSLObjC IceGridObjC Glacier2ObjC IceStormObjC
$(project)_project 		= Objective-C Test Controller.xcodeproj
$(project)_scheme  		= $(if $(filter arc-xcodesdk,$2),Objective-C ARC Test Controller,Objective-C Test Controller)
$(project)_configuration 	= $(if $(filter $(OPTIMIZE),yes),Release,Debug)

projects += $(project)
