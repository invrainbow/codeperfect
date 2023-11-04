module.exports = {
  plugins: [
    require('@csstools/postcss-sass')(),
    require('tailwindcss'),
    require('autoprefixer'),
    require('postcss-nested'),
  ],
}
