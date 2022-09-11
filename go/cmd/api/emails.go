package main

import (
	_ "embed"
)

//go:embed emails/user_created.txt
var emailUserCreatedTxt string

//go:embed emails/user_created.html
var emailUserCreatedHtml string

//go:embed emails/user_disabled.txt
var emailUserDisabledTxt string

//go:embed emails/user_disabled.html
var emailUserDisabledHtml string

//go:embed emails/user_enabled.txt
var emailUserEnabledTxt string

//go:embed emails/user_enabled.html
var emailUserEnabledHtml string
