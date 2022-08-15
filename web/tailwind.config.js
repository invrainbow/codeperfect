module.exports = {
  content: ["./src/**/*.{js,jsx,ts,tsx}", "./public/index.html"],
  darkMode: 'media',
  theme: {
    extend: {},
  },
  variants: {
    extend: {
      opacity: ["disabled", "group-hover"],
      borderWidth: ["last"],
      display: ["group-hover"],
      transform: ["hover"],
      margin: ["last"],
    },
  },
  plugins: [],
};
