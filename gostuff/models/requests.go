package models

type ErrorResponse struct {
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
	Success   bool `json:"success"`
}

type HeartbeatRequest struct {
	SessionID uint `json:"session_id"`
}

type HeartbeatResponse struct {
	Ok bool `json:"ok"`
}
