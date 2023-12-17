import { links } from "@/links";
import { IconMenu2, IconX } from "@tabler/icons-react";
import { useEffect, useState } from "react";
import { A } from "./A";
import { Icon } from "./Icon";
import { Logo } from "./Logo";

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
    <div className="p-4 md:p-6">
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
                {HEADER_LINKS.map(([url, label]) => (
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
          {HEADER_LINKS.map(([url, label]) => (
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
