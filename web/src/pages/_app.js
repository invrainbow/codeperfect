import "@/styles/globals.scss";

import { A, Icon } from "@/components";
import { inter, jetbrainsMono } from "@/components/fonts";
import { LINKS, SUPPORT_EMAIL } from "@/constants";
import { IconMenu2, IconX } from "@tabler/icons-react";
import cx from "classnames";
import { useEffect, useState } from "react";

function Logo({ onClick = undefined, hideText = undefined }) {
  return (
    <A
      href="/"
      className="font-bold text-lg text-black no-underline whitespace-nowrap inline-flex flex-shrink-0 items-center"
      onClick={onClick}
    >
      <img
        alt="logo"
        className="w-auto h-8 inline-block mr-3"
        src={"/logo.png"}
      />
      {!hideText && (
        <span className="inline-block logo text-lg font-bold">CodePerfect</span>
      )}
    </A>
  );
}

function Header() {
  const [showMenu, setShowMenu] = useState(false);

  useEffect(() => {
    const listener = () => {
      setShowMenu(false);
    };
    document.body.addEventListener("click", listener);
    return () => document.body.removeEventListener("click", listener);
  }, []);

  const links = [
    [LINKS.docs, "Docs"],
    [LINKS.download, "Download"],
    [LINKS.github, "Github"],
  ];

  return (
    <div className="p-4 md:p-6 bg-white shadow-2xl shadow-neutral-800/5">
      <div className="flex justify-between items-center w-full md:max-w-screen-lg md:mx-auto">
        <Logo />
        <div className="md:hidden relative">
          <Icon
            block
            onClick={(e) => {
              setShowMenu(!showMenu);
              e.stopPropagation();
            }}
            className="ml-2 text-3xl leading-none opacity-50"
            icon={IconMenu2}
          />
          {showMenu && (
            <div
              className="bg-neutral-900 text-black fixed top-0 left-0 right-0 p-4 border-b border-gray-200 shadow-lg z-50"
              onClick={(e) => e.stopPropagation()}
            >
              <button
                onClick={() => setShowMenu(false)}
                className="absolute z-50 top-4 right-4 w-8 flex items-center justify-center h-8 rounded-full bg-neutral-700 text-white"
              >
                <Icon icon={IconX} />
              </button>
              <div className="invert z-40 relative">
                <Logo onClick={() => setShowMenu(false)} />
              </div>
              <div className="mt-2 flex flex-col md:flex-row md:items-center">
                {links.map(([url, label]) => (
                  <A
                    key={url}
                    className="flex text-neutral-100 no-underline whitespace-nowrap md:hidden leading-none py-2 md:items-center"
                    onClick={() => setShowMenu(false)}
                    href={url}
                  >
                    {label}
                  </A>
                ))}
              </div>
            </div>
          )}
        </div>
        <div className="hidden md:flex items-center gap-x-7">
          {links.map(([url, label]) => (
            <A
              key={url}
              className="text-neutral-700 no-underline whitespace-nowrap hidden md:inline-flex"
              href={url}
            >
              {label}
            </A>
          ))}
        </div>
      </div>
    </div>
  );
}

const FOOTER_LINKS = [
  { href: LINKS.docs, label: "Docs" },
  { href: LINKS.download, label: "Download" },
  { href: LINKS.github, label: "Github" },
  { href: "/faq", label: "FAQ" },
  { href: `mailto:${SUPPORT_EMAIL}`, label: "Support" },
];

function Footer() {
  return (
    <div className="bg-white px-6 pt-6 lg:pt-12 pb-8 md:pb-24 border-t border-gray-50">
      <div className="flex flex-col-reverse md:flex-row gap-y-4 md:gap-0 hmd:flex-row justify-between w-full md:max-w-screen-lg md:mx-auto items-start">
        <div className="text-gray-500">
          <div className="opacity-50 hidden md:block">
            <Logo hideText />
          </div>
          <div>&copy; {new Date().getFullYear()} CodePerfect</div>
        </div>
        <div className="flex flex-col md:flex-row md:items-start gap-y-3 md:gap-x-8 leading-none">
          {FOOTER_LINKS.map((it) => (
            <A
              key={it.href}
              className="text-gray-800 no-underline"
              href={it.href}
            >
              {it.label}
            </A>
          ))}
        </div>
      </div>
    </div>
  );
}

export default function App({ Component, pageProps }) {
  return (
    <div
      className={cx(
        "flex flex-col min-h-screen font-sans",
        inter.variable,
        jetbrainsMono.variable
      )}
    >
      <div className="flex-grow">
        <Header />
        <Component {...pageProps} />
      </div>
      <Footer />
    </div>
  );
}
