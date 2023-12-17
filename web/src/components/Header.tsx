import { links } from "@/links";
import { useEffect, useState } from "react";
import { A } from "./A";
import { Icon } from "./Icon";
import { Logo } from "./Logo";
import { Box } from "./Box";
import { Flex } from "./Flex";

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
    <Box cx="p-4 md:p-6">
      <Box cx="flex justify-between w-full md:max-w-screen-lg md:mx-auto">
        <Logo />
        <Box cx="md:hidden relative">
          <Icon
            onClick={(e) => {
              setShowMenu(!showMenu);
              e.stopPropagation();
            }}
            cx="ml-2 text-3xl leading-none opacity-50"
            icon="Menu2"
          />
          {showMenu && (
            <Box
              cx="bg-neutral-900 text-black fixed top-0 left-0 right-0 p-4 border-b border-gray-200 shadow-lg z-50"
              onClick={(e) => e.stopPropagation()}
            >
              <Flex
                as="button"
                onClick={() => setShowMenu(false)}
                cx="absolute z-50 top-4 right-4 w-8 justify-center h-8 rounded-full bg-neutral-700 text-white"
              >
                <Icon icon="X" />
              </Flex>
              <Box cx="invert z-40 relative">
                <Logo onClick={() => setShowMenu(false)} />
              </Box>
              <Flex cx="mt-2 flex-col md:flex-row items-start md:items-center">
                {HEADER_LINKS.map(([url, label]) => (
                  <A
                    key={url}
                    cx="flex text-neutral-100 no-underline whitespace-nowrap md:hidden leading-none py-2 items-start md:items-center"
                    onClick={() => setShowMenu(false)}
                    href={url}
                  >
                    {label}
                  </A>
                ))}
              </Flex>
            </Box>
          )}
        </Box>
        <Box cx="hidden md:flex items-center gap-x-7">
          {HEADER_LINKS.map(([url, label]) => (
            <A
              key={url}
              cx="text-neutral-700 no-underline whitespace-nowrap hidden md:inline-flex"
              href={url}
            >
              {label}
            </A>
          ))}
        </Box>
      </Box>
    </Box>
  );
}
