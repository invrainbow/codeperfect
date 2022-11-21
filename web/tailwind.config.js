module.exports = {
  content: ["./src/**/*.{js,jsx,ts,tsx}", "./public/index.html"],
  darkMode: "media",
  theme: {
    extend: {
      colors: {
        black: "#000",
        white: "#fff",
        primary: "rgb(25, 168, 240)",
        gray: {
          50: "rgba(255, 255, 255, 0.05)",
          100: "rgba(255, 255, 255, 0.1)",
          200: "rgba(255, 255, 255, 0.2)",
          300: "rgba(255, 255, 255, 0.3)",
          400: "rgba(255, 255, 255, 0.4)",
          500: "rgba(255, 255, 255, 0.5)",
          600: "rgba(255, 255, 255, 0.6)",
          700: "rgba(255, 255, 255, 0.7)",
          800: "rgba(255, 255, 255, 0.8)",
          900: "rgba(255, 255, 255, 0.9)",
        },
      },
    },
    fontFamily: {
      serif: ["New York", "Georgia", "serif"],
      sans: [
        "Oxygen",
        "system-ui",
        "-apple-system",
        "BlinkMacSystemFont",
        "Segoe UI",
        "Roboto",
        "Helvetica Neue",
        "Ubuntu",
        "sans-serif",
      ],
      mono: ["Menlo", "Monaco", "Consolas", "Courier New", "monospace"],
      title: [
        "-apple-system",
        "BlinkMacSystemFont",
        "Segoe UI",
        "Roboto",
        "Helvetica Neue",
        "Ubuntu",
        "sans-serif",
      ],
    },
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
