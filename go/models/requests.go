package models

import (
	"fmt"
)

type ErrorResponse struct {
	Error string `json:"error"`
}

type UpdateResponse struct {
	NeedAutoupdate bool   `json:"need_autoupdate"`
	Version        int    `json:"version"`
	DownloadURL    string `json:"download_url"`
	DownloadHash   string `json:"download_hash"`
}

type AuthRequest struct {
	OS             string `json:"os"`
	CurrentVersion int    `json:"current_version"`
}

type AuthRequestV2 struct {
	OS string `json:"os"`
}

type AuthResponse struct {
	SessionID uint `json:"session_id"`
	Success   bool `json:"success"`
}

type AuthResult int

const (
	AuthSuccessLocked AuthResult = iota
	AuthSuccessUnlocked
	AuthFailVersionLocked
	AuthFailInvalidCredentials
	AuthFailInvalidVersion
	AuthFailUserInactive
	AuthFailOther
)

func (r *AuthResult) IsSuccess() bool {
	return *r == AuthSuccessLocked || *r == AuthSuccessUnlocked
}

func (r *AuthResult) String() string {
	switch *r {
	case AuthSuccessLocked:
		return "AuthSuccessLocked"
	case AuthSuccessUnlocked:
		return "AuthSuccessUnlocked"
	case AuthFailVersionLocked:
		return "AuthFailVersionLocked"
	case AuthFailInvalidCredentials:
		return "AuthFailInvalidCredentials"
	case AuthFailInvalidVersion:
		return "AuthFailInvalidVersion"
	case AuthFailUserInactive:
		return "AuthFailUserInactive"
	case AuthFailOther:
		return "AuthFailOther"
	}
	return fmt.Sprintf("(unknown: %d)", *r)
}

type AuthResponseV2 struct {
	Result        AuthResult `json:"result"`
	LockedVersion *int       `json:"locked_version,omitempty"`
}

type HeartbeatRequest struct {
	SessionID uint `json:"session_id"`
}

type HeartbeatResponse struct {
	Ok bool `json:"ok"`
}

type CrashReportRequest struct {
	Content string `json:"content"`
	OS      string `json:"os"`
	Version int    `json:"version"`
}

type CrashReportRequestV2 struct {
	Content string `json:"content"`
	OS      string `json:"os"`
}
