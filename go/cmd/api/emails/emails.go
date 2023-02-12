package emails

import (
	_ "embed"
)

//go:embed user_created.txt
var EmailUserCreatedText string

//go:embed user_created.html
var EmailUserCreatedHtml string

//go:embed user_disabled.txt
var EmailUserDisabledText string

//go:embed user_disabled.html
var EmailUserDisabledHtml string

//go:embed user_enabled.txt
var EmailUserEnabledText string

//go:embed user_enabled.html
var EmailUserEnabledHtml string
