package models

const (
	ErrorInternal = iota
	ErrorEmailNotFound
	ErrorUserNoLongerActive
	ErrorInvalidLicenseKey
	ErrorInvalidData
	ErrorInvalidOS
	ErrorInvalidVersion
	ErrorInvalidDownloadCode
	ErrorTrialExpired
	ErrorInvalidSession
)

var ErrorMessages = map[int]string{
	ErrorInternal:            "Internal server error.",
	ErrorEmailNotFound:       "Email not found.",
	ErrorUserNoLongerActive:  "User is no longer active.",
	ErrorInvalidLicenseKey:   "Invalid license key.",
	ErrorInvalidData:         "Invalid data.",
	ErrorInvalidOS:           "Invalid OS.",
	ErrorInvalidVersion:      "Invalid version.",
	ErrorInvalidDownloadCode: "Invalid download code.",
	ErrorTrialExpired:        "Trial has expired.",
	ErrorInvalidSession:      "Invalid session ID.",
}

type ErrorResponse struct {
	Code  int    `json:"code"`
	Error string `json:"error"`
}

type UpdateRequest struct {
	OS             string `json:"os"`
	CurrentVersion int    `json:"current_version"`
}

type UpdateResponse struct {
	NeedAutoupdate bool   `json:"need_autoupdate"`
	Version        int    `json:"version"`
	DownloadURL    string `json:"download_url"`
	DownloadHash   string `json:"string"`
}

type AuthRequest struct {
	OS             string `json:"os"`
	CurrentVersion int    `json:"current_version"`
}

type AuthResponse struct {
	SessionID uint `json:"session_id"`
}

type HeartbeatRequest struct {
	SessionID uint `json:"session_id"`
}

type HeartbeatResponse struct {
	Ok bool `json:"ok"`
}
