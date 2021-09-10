package models

const (
	ErrorInternal = iota
	ErrorEmailNotFound
	ErrorUserNoLongerActive
	ErrorInvalidLicenseKey
)

var ErrorMessages = map[int]string{
	ErrorInternal:           "Internal server error.",
	ErrorEmailNotFound:      "Email not found.",
	ErrorUserNoLongerActive: "User is no longer active.",
	ErrorInvalidLicenseKey:  "Invalid license key.",
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
