import { links } from "@/links";
import { Link } from "./Link";
import { Logo } from "./Logo";
import dom from "./dom";

const FOOTER_LINKS = [
  [links.docs, "Docs"],
  [links.download, "Download"],
  [links.github, "Github"],
  [links.faq, "FAQ"],
  [links.supportEmail, "Support"],
];

export const Footer = () => (
  <dom.div cx="px-6 pt-6 pb-8 md:py-12">
    <dom.div cx="flex items-start md:items-end flex-col-reverse md:flex-row gap-y-4 md:gap-0 hmd:flex-row justify-between w-full md:max-w-screen-lg md:mx-auto">
      <dom.div cx="flex flex-col items-start gap-2 text-gray-500">
        <dom.div cx="opacity-50 hidden md:block">
          <Logo hideText />
        </dom.div>
        <dom.div cx="leading-none">
          &copy; {new Date().getFullYear()} CodePerfect
        </dom.div>
      </dom.div>
      <dom.div cx="flex flex-col md:flex-row items-start gap-y-3 md:gap-x-8 leading-none">
        {FOOTER_LINKS.map(([href, label]) => (
          <Link key={href} cx="text-gray-800 no-underline" href={href}>
            {label}
          </Link>
        ))}
      </dom.div>
    </dom.div>
  </dom.div>
);
