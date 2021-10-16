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
}

type ErrorResponse struct {
	Code  int    `json:"code"`
	Error string `json:"error"`
}

type AuthRequest struct {
	OS             string `json:"os"`
	CurrentVersion int    `json:"current_version"`
}

type AuthResponse struct {
	NeedAutoupdate bool   `json:"need_autoupdate"`
	Version        int    `json:"version"`
	DownloadURL    string `json:"download_url"`
	DownloadHash   string `json:"string"`
}

type DownloadRequest struct {
	Code string `json:"code"`
	OS   string `json:"os"`
}

type DownloadResponse struct {
	URL        string `json:"url"`
	Email      string `json:"email"`
	LicenseKey string `json:"license_key"`
}
