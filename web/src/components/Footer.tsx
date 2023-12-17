import { links } from "@/links";
import { Logo } from "./Logo";
import { A } from "./A";
import { Box } from "./Box";
import { Flex } from "./Flex";

const FOOTER_LINKS = [
  [links.docs, "Docs"],
  [links.download, "Download"],
  [links.github, "Github"],
  [links.faq, "FAQ"],
  [links.supportEmail, "Support"],
];

export const Footer = () => (
  <Box cx="px-6 pt-6 pb-8 md:py-12">
    <Flex cx="items-start md:items-end flex-col-reverse md:flex-row gap-y-4 md:gap-0 hmd:flex-row justify-between w-full md:max-w-screen-lg md:mx-auto">
      <Box cx="flex flex-col items-start gap-2 text-gray-500">
        <Box cx="opacity-50 hidden md:block">
          <Logo hideText />
        </Box>
        <Box cx="leading-none">
          &copy; {new Date().getFullYear()} CodePerfect
        </Box>
      </Box>
      <Flex cx="flex-col md:flex-row items-start gap-y-3 md:gap-x-8 leading-none">
        {FOOTER_LINKS.map(([href, label]) => (
          <A key={href} cx="text-gray-800 no-underline" href={href}>
            {label}
          </A>
        ))}
      </Flex>
    </Flex>
  </Box>
);
