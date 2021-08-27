package models

type ErrorResponse struct {
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
