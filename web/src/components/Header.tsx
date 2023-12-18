import { links } from "@/links";
import { useEffect, useState } from "react";
import { Icon } from "./Icon";
import { Link } from "./Link";
import { Logo } from "./Logo";
import dom from "./dom";

const HEADER_LINKS = [
  [links.docs, "Docs"],
  [links.download, "Download"],
  [links.github, "Github"],
];

export function Header() {
  const [showMenu, setShowMenu] = useState(false);

  useEffect(() => {
    const listener = () => {
      setShowMenu(false);
    };
    document.body.addEventListener("click", listener);
    return () => document.body.removeEventListener("click", listener);
  }, []);

  return (
    <dom.div cx="p-4 md:p-6">
      <dom.div cx="flex justify-between w-full md:max-w-screen-lg md:mx-auto">
        <Logo />
        <dom.div cx="md:hidden relative">
          <Icon
            onClick={(e: MouseEvent) => {
              setShowMenu(!showMenu);
              e.stopPropagation();
            }}
            cx="ml-2 text-3xl leading-none opacity-50"
            icon="Menu2"
          />
          {showMenu && (
            <dom.div
              cx="bg-neutral-900 text-black fixed top-0 left-0 right-0 p-4 border-b border-gray-200 shadow-lg z-50"
              onClick={(e: MouseEvent) => e.stopPropagation()}
            >
              <dom.div
                onClick={() => setShowMenu(false)}
                cx="absolute z-50 top-4 right-4 w-8 justify-center h-8 rounded-full bg-neutral-700 text-white"
              >
                <Icon icon="X" />
              </dom.div>
              <dom.div cx="invert z-40 relative">
                <Logo onClick={() => setShowMenu(false)} />
              </dom.div>
              <dom.div cx="flex mt-2 flex-col md:flex-row items-start md:items-center">
                {HEADER_LINKS.map(([url, label]) => (
                  <Link
                    key={url}
                    cx="flex text-neutral-100 no-underline whitespace-nowrap md:hidden leading-none py-2 items-start md:items-center"
                    onClick={() => setShowMenu(false)}
                    href={url}
                  >
                    {label}
                  </Link>
                ))}
              </dom.div>
            </dom.div>
          )}
        </dom.div>
        <dom.div cx="hidden md:flex items-center gap-x-7">
          {HEADER_LINKS.map(([url, label]) => (
            <Link
              key={url}
              cx="text-neutral-700 no-underline whitespace-nowrap hidden md:inline-flex"
              href={url}
            >
              {label}
            </Link>
          ))}
        </dom.div>
      </dom.div>
    </dom.div>
  );
}
