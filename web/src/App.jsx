import { AiFillApple } from "@react-icons/all-files/ai/AiFillApple";
import { AiFillWindows } from "@react-icons/all-files/ai/AiFillWindows";
import { AiOutlineCheck } from "@react-icons/all-files/ai/AiOutlineCheck";
import { AiOutlineClose } from "@react-icons/all-files/ai/AiOutlineClose";
import { BiMenu } from "@react-icons/all-files/bi/BiMenu";
import { BsChevronRight } from "@react-icons/all-files/bs/BsChevronRight";
import { FaLinux } from "@react-icons/all-files/fa/FaLinux";
import { FaTwitter } from "@react-icons/all-files/fa/FaTwitter";
import { FaDiscord } from "@react-icons/all-files/fa/FaDiscord";
import { HiOutlineDownload } from "@react-icons/all-files/hi/HiOutlineDownload";

import posthog from "posthog-js";
import cx from "classnames";
import React from "react";
import { Helmet } from "react-helmet";
import { useMediaQuery } from "react-responsive";
import { twMerge } from "tailwind-merge";

import {
  BrowserRouter,
  Link,
  Outlet,
  Navigate,
  Route,
  Routes,
  useLocation,
} from "react-router-dom";
import _ from "lodash";
import "./index.css";

posthog.init("phc_kIt8VSMD8I2ScNhnjWDU2NmrK9kLIL3cHWpkgCX3Blw", {
  api_host: "https://app.posthog.com",
});

const SUPPORT_EMAIL = "support@codeperfect95.com";
const CURRENT_BUILD = process.env.REACT_APP_BUILD_VERSION; // "22.09.4";

const isDev = process.env.NODE_ENV === "development";

const BASE_LINKS = {
  docs: "https://docs.codeperfect95.com",
  gettingStarted: "https://docs.codeperfect95.com/getting-started",
  changelog: "https://docs.codeperfect95.com/changelog",
  issueTracker: "https://github.com/codeperfect95/issue-tracker",

  buyPersonalMonthly: "https://buy.stripe.com/aEU5kx2aTaso4TK008",
  buyPersonalYearly: "https://buy.stripe.com/fZefZb2aTdEAbi8aEN",
  buyProMonthly: "https://buy.stripe.com/6oE28ldTB5843PG9AK",
  buyProYearly: "https://buy.stripe.com/28o8wJ3eXfMI4TK5kv",

  twitter: "https://twitter.com/codeperfect95",
  discord: "https://discord.gg/WkFY44BY7a",
  mailingList: "https://exceptional-innovator-7731.ck.page/0b59fe1347",
  handmadeManifesto: "https://handmade.network/manifesto",
};

const DEV_LINKS = {
  buyPersonalMonthly: "https://buy.stripe.com/test_4gw8xrb10g8D7QsbIP",
  buyPersonalYearly: "https://buy.stripe.com/test_8wMfZT3yy2hN1s45ks",
  buyProMonthly: "https://buy.stripe.com/test_6oEdRL1qq5tZ5Ik6oy",
  buyProYearly: "https://buy.stripe.com/test_3cs6pj9WW3lR3Ac7sB",
};

function makeMergedLinks() {
  const mergedLinks = { ...BASE_LINKS };
  if (isDev) {
    _.merge(mergedLinks, DEV_LINKS);
  }
  return mergedLinks;
}

const LINKS = makeMergedLinks();

const CDN_PATH = "https://codeperfect-static.s3.us-east-2.amazonaws.com";

function asset(path) {
  return isDev ? `/public${path}` : `${CDN_PATH}${path}`;
}

function isExternalLink(href) {
  const prefixes = ["http://", "https://", "mailto:", "tel:"];
  return prefixes.some((it) => href.startsWith(it));
}

function A({ link, children, href, newWindow, ...props }) {
  if (!href || isExternalLink(href) || newWindow) {
    props.href = href;
    if (newWindow) {
      props.target = "_blank";
    }
    return <a {...props}>{children}</a>;
  }
  props.to = href;
  return <Link {...props}>{children}</Link>;
}

function wrap(elem, extraClassName, defaultProps, overrideProps) {
  return ({ children, className, ...props }) => {
    const newProps = {
      ...(defaultProps || {}),
      ...props,
      ...(overrideProps || {}),
      className: twMerge(extraClassName, className),
    };
    return React.createElement(elem, newProps, children);
  };
}

const WallOfText = wrap(
  "div",
  "wall-of-text leading-normal md:mx-auto md:max-w-2xl bg-white md:my-32 p-8 md:p-16 md:rounded-lg md:shadow text-neutral-700"
);
const Title = wrap("h2", "m-0 mb-6 font-bold text-3xl text-black");

function Icon({ block, noshift, icon: IconComponent, ...props }) {
  return (
    <span className={block ? "block" : "inline-block"}>
      <IconComponent {...props} />
    </span>
  );
}

const FEATURES = _.shuffle([
  {
    label: "Integrated Debugger",
    desc: "Full Delve integration. Debug your program from inside your IDE.",
  },
  {
    label: "Code Intelligence",
    desc: "Go to definition, find all usages, parameter hints, and autocomplete.",
  },
  {
    label: "Smart Autocomplete",
    desc: "Context-specific suggestions as you type. ",
  },
  {
    label: "Postfix Completions",
    desc: "Macros that work intelligently on your Go expressions.",
  },
  {
    label: "Global Fuzzy Selector",
    desc: "Works on files, symbols, commands, and completions.",
  },
  /*
  {
    label: "Custom GPU Renderer",
    desc: "OpenGL-based renderer. No Electron. 144 FPS, everything instant.",
  },
  */
  {
    label: "Tree-Based Navigation",
    desc: "Use the power of our integrated parser to directly walk the AST.",
  },
  {
    label: "Auto Format",
    desc: "Automatically gofmt your code on save, with zero configuration.",
  },
  {
    label: "Integrated Build",
    desc: "Build, jump to & fix each error, all with ergonomic keyboard shortcuts.",
  },
  {
    label: "Rename Identifier",
    desc: "Rename any symbol across your entire codebase.",
  },
  {
    label: "Command Palette",
    desc: "Press Primary+K to run any command or action inside CodePerfect.",
  },
  {
    label: "Generate Function",
    desc: "Take a call to a non-existent function and generate its signature.",
  },
  {
    label: "Fast Project-Wide Grep",
    desc: "Fast search (and replace) runs in milliseconds on large codebases.",
  },
  {
    label: "Manage Interfaces",
    desc: "Find implementations and interfaces and generate implementations.",
  },
  {
    label: "Organize Imports",
    desc: "Intelligently add/remove imports with our native import organizer.",
  },
  {
    label: "Manage Struct Tags",
    desc: "Add & remove struct tags automatically.",
  },
]);

const SELLING_POINTS = [
  {
    icon: asset("/icon-vim.png"),
    label: "Full Vim integration",
    content: (
      <>
        <p>
          CodePerfect provides Vim functionality as a first-class concept, not
          as an afterthought plugin. This gives you the entire Vim feature-set
          at your fingertips, integrated seamlessly with everything else.
        </p>
      </>
    ),
  },
  {
    icon: asset("/icon-basics.png"),
    label: "Back to basics",
    content: (
      <>
        <p>
          <p>
            CodePerfect is old-school about performance, and makes a conscious
            effort not to squander your CPU cycles.
          </p>
          <p>
            Instant startup. A buttery smooth 144 FPS. Instant response to every
            keystroke. An optimized indexer that gobbles through large
            codebases.
          </p>
        </p>
      </>
    ),
  },
  {
    icon: asset("/icon-bicycle.png"),
    label: "A bicycle for coding",
    content: (
      <>
        <p>
          With predictable operations, ergonomic hotkeys, and tons of cases of
          “just doing the right thing,” CodePerfect assists instead of
          interfering with your dev workflow. It does its job and gets out of
          your way.
        </p>
      </>
    ),
  },
];

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "virtual machines",
  "browsers",
  "opaque runtimes",
  "bloated abstractions",
];

function Home() {
  const getIconName = (name) => {
    return name
      .split(/[^A-Za-z]/)
      .map((it) => it.toLowerCase())
      .join("-");
  };

  return (
    <div
      className="mx-auto mt-8 md:mt-24 w-full"
      style={{ maxWidth: "1920px" }}
    >
      <div className="max-w-full text-lg leading-relaxed p-4">
        <div className="text-center text-4xl md:text-5xl mb-6 font-bold text-white leading-snug">
          A&nbsp;fast,&nbsp;powerful IDE&nbsp;for&nbsp;Go
        </div>

        <p className="md:hidden text-center">
          A power tool with a minimal resource footprint. Designed around the Go
          development workflow.
        </p>

        <p className="hidden md:display max-w-full mx-auto text-center text-xl text-neutral-100 leading-normal">
          A power tool with a minimal resource footprint.
          <br />
          Designed around the Go development workflow.
        </p>

        <div className="mt-8 text-center flex gap-4 justify-center">
          <A
            link
            href="/download"
            className="btn btn1 justify-center flex md:inline-flex text-center"
          >
            <Icon className="mr-2" icon={HiOutlineDownload} />
            Download
          </A>
          <A
            href={LINKS.docs}
            className="btn btn2 justify-center flex md:inline-flex text-center"
          >
            View Docs
            <Icon className="ml-2" icon={BsChevronRight} />
          </A>
        </div>

        <div className="md:hidden my-12">
          <A href={asset("/basics-screenshot.png")}>
            <img alt="screenshot" src={asset("/basics-screenshot.png")} />
          </A>
        </div>

        <div
          className="hidden md:block relative max-w-screen-xl mt-16 mx-auto"
          style={{ height: "450px" }}
        >
          <img
            className="max-w-full opacity-80 absolute top-0 left-0 right-0"
            alt="screenshot"
            src={asset("/basics-screenshot.png")}
          />
        </div>
      </div>

      <div className="batteries-included">
        <div className="batteries-included-child md:flex max-w-screen-xl mx-auto py-16 md:py-32">
          <div className="md:w-2/5 px-4 md:p-0 md:pr-16">
            <div className="text-4xl md:mt-24 mb-6 font-bold text-black">
              Batteries included
            </div>

            <div className="text-xl text-neutral-700 leading-relaxed">
              <p>
                CodePerfect is feature-rich and works out of the box with
                (almost) zero configuration.
              </p>
              <p>
                The power of an IDE, bundled into a software package as fast as
                Vim.
              </p>
            </div>
            <div className="mt-8">
              <A
                href={LINKS.docs}
                className="btn btn2 btn-invert justify-center flex md:inline-flex text-center"
              >
                View Docs
                <Icon className="ml-2" icon={BsChevronRight} />
              </A>
            </div>
          </div>
          <div className="flex-1 mx-auto grid grid-cols-2 md:grid-cols-3 gap-4 md:gap-6 mt-8 md:mt-0 p-4 md:p-0">
            {FEATURES.map((it) => (
              <div
                className="batteries-included-tile shadow-sm rounded-md p-3 hover:scale-105 transition-all select-none"
                key={it.label}
              >
                <div className="leading-none mb-2">
                  <img
                    className="inline-block w-auto h-6 mb-1.5"
                    src={asset(`/icon-${getIconName(it.label)}.png`)}
                    alt={it.label}
                  />
                  <div className="leading-none font-bold text-sm text-neutral-700">
                    {it.label}
                  </div>
                </div>
                <div className="text-sm text-neutral-500">{it.desc}</div>
              </div>
            ))}
          </div>
        </div>
      </div>

      <div className="back-to-basics relative">
        <div
          className="hidden md:block absolute z-0 left-0 top-0 bottom-0 overflow-hidden bg-cover bg-center"
          style={{
            backgroundImage: `url(${asset("/cutting-tree.png")})`,
            width: "calc((100% - 1280px)/2 + (1280px * 0.5))",
          }}
        />
        <div
          className="flex relative z-10 mx-auto"
          style={{ maxWidth: "1280px" }}
        >
          <div
            style={{ background: "rgba(255, 255, 255, 0.0)" }}
            className="flex-1 flex flex-col items-center justify-center"
          />
          <div className="box-border md:w-1/2 py-16 px-8 md:py-36 md:px-24 text-xl leading-relaxed border-dashed border-neutral-200">
            <p>
              {BAD_FEATURES.map((name) => (
                <div className="text-2xl md:text-3xl font-bold text-neutral-800 leading-tight">
                  No {name}.
                </div>
              ))}
            </p>
            <div className="mt-6 md:mt-12 mb-6">
              We{" "}
              <A
                className="underline underline-offset-4 decoration-2 font-semibold text-neutral-500 decoration-neutral-200 hover:text-neutral-600 hover:decoration-neutral-300"
                newWindow
                href={LINKS.handmadeManifesto}
              >
                handmade
              </A>{" "}
              the entire IDE stack from the metal up in blazing fast C/C++, into
              a barebones native app that literally just does the thing it's
              supposed to
            </div>
            <p>
              From the UI renderer to the code intelligence engine, everything
              is hand-optimized to be as performant as a video game.
            </p>
          </div>
        </div>
      </div>

      <div
        className="p-8 md:py-32 md:px-0"
        style={{ background: "rgba(0, 0, 0, 0.2)" }}
      >
        <div className="grid grid-cols-1 md:grid-cols-3 max-w-screen-lg mx-auto gap-12 md:gap-20">
          {SELLING_POINTS.map((it) => (
            <div
              key={it.label}
              style={{ borderColor: "rgba(255, 255, 255, 0.05)" }}
            >
              <img src={it.icon} alt={it.label} className="w-auto h-10 mb-2" />
              <div className="text-lg font-semibold text-white mb-4">
                {it.label}
              </div>
              <div>{it.content}</div>
            </div>
          ))}
        </div>
      </div>

      <div className="bg-white py-8 px-4 md:px-0 md:pt-24 md:pb-28 overflow-hidden border-t border-white border-2">
        <div className="max-w-screen-xl mx-auto md:flex text-neutral-700 border-white border-2">
          <div className="md:w-1/3">
            <div className="text-black font-bold text-3xl mb-6">
              Ready to get started?
            </div>
            <p className="text-xl leading-relaxed mb-8">
              Try CodePerfect for free for 7 days
              <br />
              with all features available.
            </p>
            <p>
              <A
                link
                href="/download"
                className={twMerge(
                  "btn btn1 btn-invert justify-center inline-flex text-center bg-black text-white text-lg px-6"
                )}
              >
                <Icon className="mr-2" icon={HiOutlineDownload} />
                Download
              </A>
            </p>
          </div>
          <div className="flex-1 relative hidden md:block">
            <div className="absolute -bottom-24 left-0 -right-8">
              <img
                src={asset("/basics-screenshot.png")}
                className="max-w-full h-auto opacity-100"
                alt="screenshot"
                // style={{ marginBottom: "-10%" }}
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function PaymentDone() {
  return (
    <WallOfText>
      <Title>Thanks for the purchase!</Title>
      <p>
        Please check your email &mdash; we just sent your license key to you. If
        you don't see it, see if it's in your spam folder. If you still don't
        see it, please <a href={`mailto:${SUPPORT_EMAIL}`}>contact support</a>.
      </p>
      <p>
        If you haven't done so yet, you can download CodePerfect{" "}
        <A href="/download">here</A>.
      </p>
    </WallOfText>
  );
}

function PortalDone() {
  return (
    <WallOfText>
      <p className="text-center">
        You just left the billing portal. You can now close this window.
      </p>
    </WallOfText>
  );
}

const BuyLicenseBox = wrap(
  "div",
  "w-auto overflow-hidden border border-neutral-300 rounded-md shadow-sm"
);

const disableButtonProps = {
  onClick: (e) => {
    e.preventDefault();
    alert(
      "CodePerfect is currently in maintenance mode and is not available for download. Please check back later."
    );
  },
};

const BuyLicenseButton = wrap(
  A,
  "btn btn1 btn-invert p-3 block text-center",
  null
  // { ...disableButtonProps, href: "#" }
);

function BuyLicenseButtons({ monthly, monthlyLink, yearly, yearlyLink }) {
  const options = [
    [monthly, monthlyLink, "Monthly", "month"],
    [yearly, yearlyLink, "Yearly", "year"],
  ];

  return (
    <div className="p-5 border-t border-neutral-200 bg-neutral-100">
      <div className="grid grid-cols-2 gap-3">
        {options.map(([amount, link, unit, unit2]) => (
          <div>
            <div className="flex items-center mb-2.5">
              <div className="leading-none text-xl font-bold text-neutral-900">
                ${amount}
              </div>
              <div className="text-neutral-600 leading-none text-sm relative top-0.5 ml-1">
                /{unit2}
              </div>
            </div>
            <BuyLicenseButton href={link}>Buy {unit}</BuyLicenseButton>
          </div>
        ))}
      </div>
    </div>
  );
}

function BuyLicense() {
  return (
    <WallOfText className="md:max-w-3xl">
      <Title>Buy CodePerfect</Title>
      <div className="my-6 grid grid-cols-1 sm:grid-cols-2 gap-6">
        <BuyLicenseBox>
          <div className="p-5">
            <h1 className="font-bold text-neutral-900 mb-2">Personal</h1>
            <PricingPoint label="Commercial use ok" />
            <PricingPoint label="All features available" />
            <PricingPoint not label="Company can't pay" />
            <PricingPoint not label="Purchase can't be expensed" />
          </div>
          <BuyLicenseButtons
            monthly={5}
            yearly={50}
            monthlyLink={LINKS.buyPersonalMonthly}
            yearlyLink={LINKS.buyPersonalYearly}
          />
        </BuyLicenseBox>
        <BuyLicenseBox>
          <div className="p-5">
            <h1 className="font-bold text-neutral-900 mb-2">Pro</h1>
            <PricingPoint label="Commercial use ok" />
            <PricingPoint label="All features available" />
            <PricingPoint label="Company can pay" />
            <PricingPoint label="Purchase can be expensed" />
          </div>
          <BuyLicenseButtons
            monthly={10}
            yearly={100}
            monthlyLink={LINKS.buyProMonthly}
            yearlyLink={LINKS.buyProYearly}
          />
        </BuyLicenseBox>
      </div>
      <p>
        We'll send your license key to the email you provide during checkout.
      </p>
      <p>
        Licenses are per-user. If you want to buy licenses in bulk, or have any
        other custom requests, please{" "}
        <a href={`mailto:${SUPPORT_EMAIL}`}>contact support</a>.
      </p>
    </WallOfText>
  );
}

function Download() {
  const links = [
    {
      platform: "windows-x64",
      icon: AiFillWindows,
      label: "Windows",
    },
    {
      platform: "mac-x64",
      icon: AiFillApple,
      label: "macOS Intel",
    },
    {
      platform: "mac-arm",
      icon: AiFillApple,
      label: "macOS M1",
    },
    {
      platform: "linux-x64",
      icon: FaLinux,
      label: "Linux",
      disabledText: "Coming soon!",
    },
  ];

  return (
    <div className="my-12 md:my-28">
      <div className="font-bold px-4 md:text-center text-3xl text-white">
        Download CodePerfect {CURRENT_BUILD}
      </div>
      <div className="max-w-3xl mx-auto mt-8 px-4 md:text-center">
        <p className="flex flex-wrap flex-col md:flex-row gap-2 justify-center">
          {links.map((it) => (
            <A
              href={
                it.disabledText
                  ? "#"
                  : `https://codeperfect95.s3.us-east-2.amazonaws.com/app/${it.platform}-${CURRENT_BUILD}.zip`
              }
              className={cx("btn", it.disabledText ? "btn2 disabled" : "btn1")}
              title={it.disabledText}
              onClick={(e) => {
                // disableButtonProps.onClick(e);
                posthog.capture("download", { platform: it.platform });
              }}
            >
              <Icon className="mr-1" icon={it.icon} />
              {it.label}
            </A>
          ))}
        </p>
        <p>
          CodePerfect is free to evaluate for 7 days. After that you'll need a{" "}
          <A href="/buy">license</A> for continued use.
        </p>
      </div>
      <div className="px-4 mt-12 md:mt-20">
        <div
          style={{ maxWidth: "calc(min(100%, 1024px))" }}
          className="mx-auto items-center gap-8 my-8 hidden md:flex rounded-xl overflow-hidden border border-gray-300 shadow-md"
        >
          <img
            alt="screenshot"
            className="max-w-full max-h-full w-auto h-auto"
            src={asset("/download.png")}
          />
        </div>
      </div>
      <div className="flex-grow"></div>
    </div>
  );
}

function Terms() {
  return (
    <WallOfText>
      <Title>Terms of Service</Title>
      <p>
        This website provides you with information about the IDE and a means for
        you to subscribe to our services, which allow you to use the IDE for as
        long as your subscription is active.
      </p>
      <p>
        The IDE is an application that lets you write Go applications. In
        exchange for paying a monthly rate, we provide you with a license to use
        it.
      </p>

      <div className="mt-16">
        <Title>Privacy Policy</Title>
        <p>
          When you sign up, we collect your name, email, and credit card
          information. We use this information to bill you and send you emails
          with updates about your payment status (for example, if your card
          fails).
        </p>
        <p>
          The IDE contacts the server to authenticate your license key and to
          install automatic updates. This exposes your IP address to us. We
          won't share it with anyone, unless ordered to by law.
        </p>
      </div>
    </WallOfText>
  );
}

function ScrollToTop() {
  const { pathname, hash } = useLocation();

  React.useEffect(() => {
    if (!hash) {
      window.scrollTo(0, 0);
    }
  }, [pathname, hash]);

  return null;
}

function PricingPoint({ label, not }) {
  return (
    <div
      className={cx(
        "flex items-start space-x-1 leading-6 mb-1 last:mb-0",
        not && "text-red-400"
      )}
    >
      <Icon
        className="relative top-1 transform scale-90 origin-center"
        icon={not ? AiOutlineClose : AiOutlineCheck}
      />
      <span>{label}</span>
    </div>
  );
}

const NavA = wrap(
  A,
  "text-white no-underline whitespace-nowrap hidden md:inline-block"
);

const MenuA = wrap(
  A,
  "block text-neutral-200 no-underline whitespace-nowrap md:hidden leading-none py-2"
);

function Logo() {
  return (
    <A
      href="/"
      className="font-bold text-lg text-white no-underline whitespace-nowrap flex flex-shrink-0 items-center"
    >
      <img
        alt="logo"
        className="w-auto h-8 inline-block mr-3 invert"
        src={asset("/logo.png")}
      />
      <span className="inline-block logo text-lg">CodePerfect 95</span>
    </A>
  );
}

function Header() {
  const [showMenu, setShowMenu] = React.useState(true);

  React.useEffect(() => {
    const listener = (e) => {
      setShowMenu(false);
    };
    document.body.addEventListener("click", listener);
    return () => document.body.removeEventListener("click", listener);
  }, []);

  return (
    <div className="p-4 md:p-4 border-b border-gray-50">
      <div className="flex justify-between items-center w-full md:max-w-screen-lg md:mx-auto text-lg">
        <Logo />
        <div className="md:hidden relative">
          <Icon
            block
            onClick={(e) => {
              setShowMenu(!showMenu);
              e.stopPropagation();
            }}
            className="ml-2 text-4xl leading-none"
            icon={BiMenu}
          />
          {showMenu && (
            <div
              className="bg-black text-black fixed top-0 left-0 right-0 p-4 border-b border-gray-200 shadow-lg"
              onClick={(e) => e.stopPropagation()}
            >
              <button
                onClick={() => setShowMenu(false)}
                className="absolute top-4 right-4 w-8 flex items-center justify-center h-8 rounded-full bg-neutral-700 text-white"
              >
                <Icon icon={AiOutlineClose} />
              </button>
              <Logo />
              <div className="mt-2">
                <MenuA href={LINKS.docs}>Docs</MenuA>
                <MenuA href={LINKS.changelog}>Changelog</MenuA>
                <MenuA href={LINKS.discord}>Discord</MenuA>
                <MenuA href="/buy">Buy</MenuA>
                <MenuA href="/download">Download</MenuA>
              </div>
            </div>
          )}
        </div>
        <div className="hidden md:flex items-baseline space-x-6">
          <NavA href={LINKS.docs}>Docs</NavA>
          <NavA href={LINKS.changelog}>Changelog</NavA>
          <NavA href={LINKS.discord}>Discord</NavA>
          <NavA href="/buy">Buy</NavA>
          <NavA href="/download">Download</NavA>
        </div>
      </div>
    </div>
  );
}

const FootSection = wrap("div", "flex flex-col gap-y-3 md:gap-y-3 text-left");
const FootLink = wrap(A, "text-gray-800 no-underline");

function Footer() {
  return (
    <div className="px-4 pt-6 lg:pt-12 pb-8 md:pb-24 border-t border-gray-100 md:border-0">
      <div className="flex flex-col md:flex-row gap-y-4 md:gap-0 hmd:flex-row justify-between w-full md:max-w-screen-xl md:mx-auto items-start">
        <div className="text-gray-500 ">
          <span>&copy; {new Date().getFullYear()} CodePerfect 95</span>
        </div>
        <div className="flex flex-col md:flex-row md:items-start gap-y-3 md:gap-x-14 leading-none">
          <FootSection>
            <FootLink href="/buy">Buy License</FootLink>
            <FootLink href="/download">Download</FootLink>
          </FootSection>
          <FootSection>
            <FootLink href={LINKS.docs}>Docs</FootLink>
            <FootLink href={LINKS.changelog}>Changelog</FootLink>
            <FootLink href={LINKS.issueTracker}>Issue Tracker</FootLink>
          </FootSection>
          <FootSection>
            <FootLink href={`mailto:${SUPPORT_EMAIL}`}>Support</FootLink>
            <FootLink href="/terms">Terms &amp; Privacy</FootLink>
            <FootLink href={LINKS.mailingList}>Newsletter</FootLink>
          </FootSection>
          <div className="flex gap-x-4 text-2xl mt-3 md:mt-0">
            <A className="text-gray-800" href={LINKS.discord}>
              <Icon block icon={FaDiscord} />
            </A>
            <A className="text-gray-800" href={LINKS.twitter}>
              <Icon block icon={FaTwitter} />
            </A>
          </div>
        </div>
      </div>
    </div>
  );
}

function Layout() {
  return (
    <div className="flex flex-col min-h-screen">
      <div className="flex-grow">
        <ScrollToTop />
        <Header />
        <Outlet />
      </div>
      <Footer />
    </div>
  );
}

function App() {
  return (
    <>
      <Helmet>
        <meta charSet="utf-8" />
        <title>CodePerfect 95</title>
      </Helmet>
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<Layout />}>
            <Route path="download" element={<Download />} />
            <Route path="buy" element={<BuyLicense />} />
            <Route path="payment-done" element={<PaymentDone />} />
            <Route path="portal-done" element={<PortalDone />} />
            <Route path="terms" element={<Terms />} />
            <Route path="privacy" element={<Navigate to="terms" />} />
            <Route path="/" element={<Home />} />
            <Route path="*" element={<Navigate to="/" />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </>
  );
}

export default App;
