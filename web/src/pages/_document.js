import { inter, jetbrainsMono } from "@/components/fonts";
import { Html, Head, Main, NextScript } from "next/document";
import cx from "classnames";

export default function Document() {
  return (
    <Html lang="en" className={cx(inter.variable, jetbrainsMono.variable)}>
      <Head />
      <body>
        <Main />
        <NextScript />
      </body>
    </Html>
  );
}
