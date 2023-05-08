package emails

import (
	_ "embed"
)

//go:embed new_user.txt
var EmailNewUserText string

//go:embed new_user.html
var EmailNewUserHtml string
