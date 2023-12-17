import { links } from "@/links";
import { Logo } from "./Logo";
import { A } from "./A";

const FOOTER_LINKS = [
  { href: links.docs, label: "Docs" },
  { href: links.download, label: "Download" },
  { href: links.github, label: "Github" },
  { href: links.faq, label: "FAQ" },
  { href: links.supportEmail, label: "Support" },
];

export const Footer = () => (
  <div className="px-6 pt-6 pb-8 md:py-12">
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
