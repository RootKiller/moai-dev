// Copyright (c) 2010-2011 Zipline Games, Inc. All Rights Reserved.
// http://getmoai.com

#include "pch.h"

#include <moai-sim/MOAIGfxDevice.h>
#include <moai-sim/MOAIGfxResource.h>
#include <moai-sim/MOAIGfxResourceMgr.h>
#include <moai-sim/MOAIRenderMgr.h>

//================================================================//
// lua
//================================================================//

//----------------------------------------------------------------//
/**	@lua	getAge
	@text	Returns the 'age' of the graphics resource. The age
			is the number of times MOAIRenderMgr has rendered a scene
			since the resource was last bound. It is part of the
			render count, not a timestamp. This may change to be
			time-based in future releases.
 
	@in		MOAIGfxResource self
	@out	number age
*/
int MOAIGfxResource::_getAge ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAIGfxResource, "U" )

	u32 age = MOAIRenderMgr::Get ().GetRenderCounter () - self->mLastRenderCount;
	lua_pushnumber ( state, age );

	return 1;
}

//----------------------------------------------------------------//
// TODO: doxygen
int MOAIGfxResource::_getResourceState ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAIGfxResource, "U" )

	return 1;
}

//----------------------------------------------------------------//
// TODO: doxygen
int MOAIGfxResource::_preload ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAIGfxResource, "U" )

	return 0;
}

//----------------------------------------------------------------//
/**	@lua	purge
	@text	Attempt to release the resource. Generally this is used when
			responding to a memory warning from the system. A resource
			will only be released if it is renewable (i.e. has a renew
			callback or contains all information needed to reload the
			resources on demand).
			
			Using soft release can save an app in extreme memory
			circumstances, but may trigger reloads of resources during
			runtime which can significantly degrade performance.
 
	@in		MOAIGfxResource self
	@opt	number age				Release only if the texture hasn't been used in X frames.
	@out	boolean released		True if the texture was actually released.
*/
int MOAIGfxResource::_purge ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAIGfxResource, "U" )

	int age = state.GetValue < int >( 2, 0 );
	lua_pushboolean ( L, self->Purge ( age ));

	return 1;
}

//----------------------------------------------------------------//
// TODO: doxygen
int MOAIGfxResource::_scheduleForGPUCreate ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAIGfxResource, "U" )

	return 0;
}

//----------------------------------------------------------------//
// TODO: doxygen
int MOAIGfxResource::_setReloader ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAIGfxResource, "U" )

	self->mReloader.SetRef ( *self, state, 2 );
	self->InvokeLoader ();
	return 0;
}

//================================================================//
// MOAIGfxResource
//================================================================//

//----------------------------------------------------------------//
bool MOAIGfxResource::Bind () {

//	if ( !MOAIGfxDevice::Get ().GetHasContext ()) {
//		MOAILog ( 0, MOAILogMessages::MOAIGfxResource_MissingDevice );
//		return false;
//	}

	if ( this->mState != STATE_READY_TO_BIND ) {

		if (( this->mState == STATE_UNINITIALIZED ) || ( this->mState == STATE_ERROR )) return false;

		if ( !this->DoGPUCreate ()) return false;
		
		this->mLastRenderCount = MOAIRenderMgr::Get ().GetRenderCounter ();
	}

	this->OnGPUBind ();
	return true;
}

//----------------------------------------------------------------//
void MOAIGfxResource::Destroy () {

	if ( MOAIGfxResourceMgr::IsValid ()) {
		this->OnGPUDestroy ();
	}
	this->OnGPULost ();
	this->OnCPUDestroy ();
	this->mState = STATE_UNINITIALIZED;
}

//----------------------------------------------------------------//
bool MOAIGfxResource::DoCPUCreate () {

	if ( this->mState == STATE_READY_TO_BIND ) return true;
	if (( this->mState == STATE_ERROR ) || ( this->mState == STATE_UNINITIALIZED )) return false;

	// whether or not GPU is deferred, do the CPU part
	if ( this->mState == STATE_READY_FOR_CPU_CREATE ) {
	
		bool created = this->InvokeLoader () ? true : this->OnCPUCreate ();
		this->mState = created ? STATE_READY_FOR_GPU_CREATE : STATE_ERROR;
	}
	return this->mState != STATE_ERROR;
}

//----------------------------------------------------------------//
bool MOAIGfxResource::DoGPUCreate () {

	if ( this->mState == STATE_READY_TO_BIND ) return true;
	if (( this->mState == STATE_ERROR ) || ( this->mState == STATE_UNINITIALIZED )) return false;

	// if we get here, load both CPU and GPU parts

	if ( this->mState == STATE_READY_FOR_CPU_CREATE ) {
		this->mState = this->OnCPUCreate () ? STATE_READY_FOR_GPU_CREATE : STATE_ERROR;
	}

	if ( this->mState == STATE_READY_FOR_GPU_CREATE ) {
		this->mState = this->OnGPUCreate () ? STATE_READY_TO_BIND : STATE_ERROR;
		if ( this->mState == STATE_READY_TO_BIND ) {
			this->OnCPUDestroy ();
		}
	}
	return this->mState == STATE_READY_TO_BIND;
}

//----------------------------------------------------------------//
void MOAIGfxResource::FinishInit () {

	this->mState = STATE_READY_FOR_CPU_CREATE;
	this->DoCPUCreate ();
}

//----------------------------------------------------------------//
bool MOAIGfxResource::HasReloader () {

	return ( bool )this->mReloader;
}

//----------------------------------------------------------------//
bool MOAIGfxResource::InvokeLoader () {

	if ( this->mReloader ) {
		MOAIScopedLuaState state = MOAILuaRuntime::Get ().State ();
		if ( this->mReloader.PushRef ( state )) {
			state.DebugCall ( 0, 0 );
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------//
MOAIGfxResource::MOAIGfxResource () :
	mState ( STATE_UNINITIALIZED ),
	mLastRenderCount ( 0 ) {

	RTTI_SINGLE ( MOAILuaObject )

	this->mLink.Data ( this );
	
	MOAIGfxResourceMgr::Get ().InsertGfxResource ( *this );
}

//----------------------------------------------------------------//
MOAIGfxResource::~MOAIGfxResource () {

	if ( MOAIGfxResourceMgr::IsValid ()) {
		MOAIGfxResourceMgr::Get ().RemoveGfxResource ( *this );
	}
	this->mReloader.Clear ();
}

//----------------------------------------------------------------//
void MOAIGfxResource::RegisterLuaClass ( MOAILuaState& state ) {
	UNUSED ( state );
	
	state.SetField ( -1, "STATE_UNINITIALIZED",					( u32 )STATE_UNINITIALIZED );
	state.SetField ( -1, "STATE_READY_FOR_CPU_CREATE",			( u32 )STATE_READY_FOR_CPU_CREATE );
	state.SetField ( -1, "STATE_READY_FOR_GPU_CREATE",			( u32 )STATE_READY_FOR_GPU_CREATE );
	state.SetField ( -1, "STATE_READY_TO_BIND",					( u32 )STATE_READY_TO_BIND );
	state.SetField ( -1, "STATE_SCHEDULED_FOR_GPU_CREATE",		( u32 )STATE_SCHEDULED_FOR_GPU_CREATE );
	state.SetField ( -1, "STATE_ERROR",							( u32 )STATE_ERROR );
}

//----------------------------------------------------------------//
void MOAIGfxResource::RegisterLuaFuncs ( MOAILuaState& state ) {

	luaL_Reg regTable [] = {
		{ "getAge",					_getAge },
		{ "getResourceState",		_getResourceState },
		{ "preload",				_preload },
		{ "purge",					_purge },
		{ "softRelease",			_purge }, // back compat
		{ "scheduleForGPUCreate",	_scheduleForGPUCreate },
		{ "setReloader",			_setReloader },
		{ NULL, NULL }
	};
	luaL_register ( state, 0, regTable );
}

//----------------------------------------------------------------//
bool MOAIGfxResource::Purge ( u32 age ) {

	if ( this->mState == STATE_ERROR ) return false;

	age = MOAIRenderMgr::Get ().GetRenderCounter () - age;

	if ( this->mLastRenderCount <= age ) {
		this->OnCPUDestroy ();
		this->OnGPUDestroy ();
		this->mState = STATE_READY_FOR_CPU_CREATE;
		return true;
	}
	return false;
}

//----------------------------------------------------------------//
void MOAIGfxResource::Renew () {

	// any (valid) state other than error we go back to square zero
	if ( !(( this->mState == STATE_UNINITIALIZED ) || ( this->mState == STATE_ERROR ))) {
	
		this->OnGPULost (); // clear out the resource id (if any)
		this->mState = STATE_READY_FOR_CPU_CREATE;
	
		this->DoGPUCreate ();
	}
}

//----------------------------------------------------------------//
bool MOAIGfxResource::ScheduleForGPUCreate () {

	if ( this->mState == STATE_READY_TO_BIND ) return true;
	if (( this->mState == STATE_UNINITIALIZED ) || ( this->mState == STATE_ERROR )) return false;
	
	if ( this->mState != STATE_SCHEDULED_FOR_GPU_CREATE ) {
		MOAIGfxResourceMgr::Get ().ScheduleGPUAffirm ( *this );
		this->mState = STATE_SCHEDULED_FOR_GPU_CREATE;
	}
	return true;
}

//----------------------------------------------------------------//
void MOAIGfxResource::Unbind () {

	this->OnGPUUnbind ();
}
