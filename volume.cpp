#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>

static	UINT m_nNumMixers;
static	HMIXER m_hMixer;
static	MIXERCAPS m_mxcaps;

static	DWORD m_dwMinimum, m_dwMaximum;
static	DWORD m_dwVolumeControlID;
static  DWORD m_dwMuteControlID;

BOOL amdUninitialize();
BOOL amdInitialize();
BOOL amdGetMasterVolumeControl();
BOOL amdGetMasterVolumeValue(DWORD &dwVal);
BOOL amdSetMasterVolumeValue(DWORD dwVal);
void Adjust_Volume(int x);

BOOL muteInitialize();
BOOL muteUninitialize();
BOOL amdGetMasterMuteControl();
BOOL amdGetMasterMuteValue(LONG &lVal);
BOOL amdSetMasterMuteValue(LONG lVal);
void Toggle_Mute();

BOOL amdInitialize()
{
	// get the number of mixer devices present in the system
	m_nNumMixers = mixerGetNumDevs();

	m_hMixer = NULL;
	ZeroMemory(&m_mxcaps, sizeof(MIXERCAPS));

	// open the first mixer
	// A "mapper" for audio mixer devices does not currently exist.
	if (m_nNumMixers != 0)
	{
		if (mixerOpen(&m_hMixer,
						0,
						NULL,
						NULL,
						MIXER_OBJECTF_MIXER)
			!= MMSYSERR_NOERROR)
			return FALSE;

		if (mixerGetDevCaps((UINT)m_hMixer, &m_mxcaps, sizeof(MIXERCAPS))
			!= MMSYSERR_NOERROR)
			return FALSE;
	}

	return TRUE;
}

BOOL amdUninitialize()
{
	BOOL bSucc = TRUE;

	if (m_hMixer != NULL)
	{
		bSucc = mixerClose(m_hMixer) == MMSYSERR_NOERROR;
		m_hMixer = NULL;
	}

	return bSucc;
}

BOOL amdGetMasterVolumeControl()
{
	if (m_hMixer == NULL)
		return FALSE;

	// get dwLineID
	MIXERLINE mxl;
	mxl.cbStruct = sizeof(MIXERLINE);
	mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
	if (mixerGetLineInfo((HMIXEROBJ)m_hMixer,
						   &mxl,
						   MIXER_OBJECTF_HMIXER |
						   MIXER_GETLINEINFOF_COMPONENTTYPE)
		!= MMSYSERR_NOERROR)
		return FALSE;

	// get dwControlID
	MIXERCONTROL mxc;
	MIXERLINECONTROLS mxlc;
	mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
	mxlc.dwLineID = mxl.dwLineID;
	mxlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
	mxlc.cControls = 1;
	mxlc.cbmxctrl = sizeof(MIXERCONTROL);
	mxlc.pamxctrl = &mxc;
	if (mixerGetLineControls((HMIXEROBJ)m_hMixer,
							   &mxlc,
							   MIXER_OBJECTF_HMIXER |
							   MIXER_GETLINECONTROLSF_ONEBYTYPE)
		!= MMSYSERR_NOERROR)
		return FALSE;

	// record dwControlID
	m_dwMinimum = mxc.Bounds.dwMinimum;
	m_dwMaximum = mxc.Bounds.dwMaximum;
	m_dwVolumeControlID = mxc.dwControlID;

	return TRUE;
}

BOOL amdGetMasterVolumeValue(DWORD &dwVal)
{
	if (m_hMixer == NULL)
		return FALSE;

	MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
	MIXERCONTROLDETAILS mxcd;
	mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
	mxcd.dwControlID = m_dwVolumeControlID;
	mxcd.cChannels = 1;
	mxcd.cMultipleItems = 0;
	mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	mxcd.paDetails = &mxcdVolume;
	if (mixerGetControlDetails((HMIXEROBJ)m_hMixer,
								 &mxcd,
								 MIXER_OBJECTF_HMIXER |
								 MIXER_GETCONTROLDETAILSF_VALUE)
		!= MMSYSERR_NOERROR)
		return FALSE;
	
	dwVal = mxcdVolume.dwValue;

	return TRUE;
}

BOOL amdSetMasterVolumeValue(DWORD dwVal)
{
	if (m_hMixer == NULL)
		return FALSE;

	MIXERCONTROLDETAILS_UNSIGNED mxcdVolume = { dwVal };
	MIXERCONTROLDETAILS mxcd;
	mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
	mxcd.dwControlID = m_dwVolumeControlID;
	mxcd.cChannels = 1;
	mxcd.cMultipleItems = 0;
	mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	mxcd.paDetails = &mxcdVolume;
	if (mixerSetControlDetails((HMIXEROBJ)m_hMixer,
								 &mxcd,
								 MIXER_OBJECTF_HMIXER |
								 MIXER_SETCONTROLDETAILSF_VALUE)
		!= MMSYSERR_NOERROR)
		return FALSE;
	
	return TRUE;
}

void Adjust_Volume(int x)
{
	DWORD vol;
	int iLeft, iRight;

	if (FALSE == amdInitialize())
	{
		return;
	}
	
	amdGetMasterVolumeControl();
	amdGetMasterVolumeValue(vol);

	x=(int)(x*2.55);
	iLeft=(vol&0xff00)>>8;
	iRight=vol&0xff;
	iLeft+=x;
	if (iLeft>0xff) iLeft=0xff;
	if (iLeft < 0) iLeft=0;
	iRight+=x;
	if (iRight>0xff) iRight=0xff;
	if (iRight <0) iRight=0;
	vol=iLeft<<8 | iRight;

	amdSetMasterVolumeValue(vol);

	amdUninitialize();
}

BOOL muteInitialize()
{
	// get the number of mixer devices present in the system
	m_nNumMixers = mixerGetNumDevs();

	m_hMixer = NULL;
	ZeroMemory(&m_mxcaps, sizeof(MIXERCAPS));

	// open the first mixer
	// A "mapper" for audio mixer devices does not currently exist.
	if (m_nNumMixers != 0)
	{
		if (mixerOpen(&m_hMixer,
						0,
						NULL,
						NULL,
						MIXER_OBJECTF_MIXER | CALLBACK_WINDOW)
			!= MMSYSERR_NOERROR)
			return FALSE;

		if (mixerGetDevCaps((UINT)m_hMixer, &m_mxcaps, sizeof(MIXERCAPS))
			!= MMSYSERR_NOERROR)
			return FALSE;
	}

	return TRUE;
}

BOOL muteUninitialize()
{
	BOOL bSucc = TRUE;

	if (m_hMixer != NULL)
	{
		bSucc = mixerClose(m_hMixer) == MMSYSERR_NOERROR;
		m_hMixer = NULL;
	}

	return bSucc;
}

BOOL amdGetMasterMuteControl()
{
	if (m_hMixer == NULL)
		return FALSE;

	// get dwLineID
	MIXERLINE mxl;
	mxl.cbStruct = sizeof(MIXERLINE);
	mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
	if (::mixerGetLineInfo((HMIXEROBJ)m_hMixer,
						   &mxl,
						   MIXER_OBJECTF_HMIXER |
						   MIXER_GETLINEINFOF_COMPONENTTYPE)
		!= MMSYSERR_NOERROR)
		return FALSE;

	// get dwControlID
	MIXERCONTROL mxc;
	MIXERLINECONTROLS mxlc;
	mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
	mxlc.dwLineID = mxl.dwLineID;
	mxlc.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
	mxlc.cControls = 1;
	mxlc.cbmxctrl = sizeof(MIXERCONTROL);
	mxlc.pamxctrl = &mxc;
	if (::mixerGetLineControls((HMIXEROBJ)m_hMixer,
							   &mxlc,
							   MIXER_OBJECTF_HMIXER |
							   MIXER_GETLINECONTROLSF_ONEBYTYPE)
		!= MMSYSERR_NOERROR)
		return FALSE;

	// record dwControlID
	m_dwMuteControlID = mxc.dwControlID;

	return TRUE;
}

BOOL amdGetMasterMuteValue(LONG &lVal)
{
	if (m_hMixer == NULL)
		return FALSE;

	MIXERCONTROLDETAILS_BOOLEAN mxcdMute;
	MIXERCONTROLDETAILS mxcd;
	mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
	mxcd.dwControlID = m_dwMuteControlID;
	mxcd.cChannels = 1;
	mxcd.cMultipleItems = 0;
	mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	mxcd.paDetails = &mxcdMute;
	if (mixerGetControlDetails((HMIXEROBJ)m_hMixer,
								 &mxcd,
								 MIXER_OBJECTF_HMIXER |
								 MIXER_GETCONTROLDETAILSF_VALUE)
		!= MMSYSERR_NOERROR)
		return FALSE;
	
	lVal = mxcdMute.fValue;

	return TRUE;
}

BOOL amdSetMasterMuteValue(LONG lVal)
{
	if (m_hMixer == NULL)
		return FALSE;

	MIXERCONTROLDETAILS_BOOLEAN mxcdMute = { lVal };
	MIXERCONTROLDETAILS mxcd;
	mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
	mxcd.dwControlID = m_dwMuteControlID;
	mxcd.cChannels = 1;
	mxcd.cMultipleItems = 0;
	mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	mxcd.paDetails = &mxcdMute;
	if (mixerSetControlDetails((HMIXEROBJ)m_hMixer,
								 &mxcd,
								 MIXER_OBJECTF_HMIXER |
								 MIXER_SETCONTROLDETAILSF_VALUE)
		!= MMSYSERR_NOERROR)
		return FALSE;
	
	return TRUE;
}

void Toggle_Mute()
{
	LONG lVal;

	if (FALSE == muteInitialize())
	{
		return;
	}
	
	
	amdGetMasterMuteControl();

	amdGetMasterMuteValue(lVal);
	lVal=(!lVal);
	amdSetMasterMuteValue(lVal);
	
	muteUninitialize();

}

