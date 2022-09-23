import { AiFillApple } from "@react-icons/all-files/ai/AiFillApple";
import { AiFillWindows } from "@react-icons/all-files/ai/AiFillWindows";
import { AiOutlineCheck } from "@react-icons/all-files/ai/AiOutlineCheck";
import { AiOutlineClose } from "@react-icons/all-files/ai/AiOutlineClose";
import { BsChevronRight } from "@react-icons/all-files/bs/BsChevronRight";
import { BsCodeSlash } from "@react-icons/all-files/bs/BsCodeSlash";
import { FaLayerGroup } from "@react-icons/all-files/fa/FaLayerGroup";
import { FaLinux } from "@react-icons/all-files/fa/FaLinux";
import { FaTwitter } from "@react-icons/all-files/fa/FaTwitter";
import { FaDiscord } from "@react-icons/all-files/fa/FaDiscord";
import { FaRobot } from "@react-icons/all-files/fa/FaRobot";
import { GoPackage } from "@react-icons/all-files/go/GoPackage";
import { GrInfo } from "@react-icons/all-files/gr/GrInfo";
import { IoMdSearch } from "@react-icons/all-files/io/IoMdSearch";
import { HiOutlineDownload } from "@react-icons/all-files/hi/HiOutlineDownload";
import { ImMagicWand } from "@react-icons/all-files/im/ImMagicWand";
import { SiVim } from "@react-icons/all-files/si/SiVim";

import posthog from "posthog-js";
import cx from "classnames";
import React from "react";
import { Helmet } from "react-helmet";
import { useMediaQuery } from "react-responsive";
import {
  BrowserRouter,
  Link,
  Outlet,
  Navigate,
  Route,
  Routes,
  useLocation,
} from "react-router-dom";
import "./index.css";

posthog.init("phc_kIt8VSMD8I2ScNhnjWDU2NmrK9kLIL3cHWpkgCX3Blw", {
  api_host: "https://app.posthog.com",
});

const SUPPORT_EMAIL = "support@codeperfect95.com";
const CURRENT_BUILD = "22.09.3";

const isDev = process.env.NODE_ENV === "development";

const LINKS = {
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
};

const DEV_LINKS = {
  buyPersonalMonthly: "https://buy.stripe.com/test_4gw8xrb10g8D7QsbIP",
  buyPersonalYearly: "https://buy.stripe.com/test_8wMfZT3yy2hN1s45ks",
  buyProMonthly: "https://buy.stripe.com/test_6oEdRL1qq5tZ5Ik6oy",
  buyProYearly: "https://buy.stripe.com/test_3cs6pj9WW3lR3Ac7sB",
};

function getLink(key) {
  if (isDev) {
    if (DEV_LINKS[key]) {
      return DEV_LINKS[key];
    }
  }
  return LINKS[key];
}

const CDN_PATH = "https://codeperfect-static.s3.us-east-2.amazonaws.com";

function asset(path) {
  return isDev ? `/public${path}` : `${CDN_PATH}${path}`;
}

function isExternalLink(href) {
  const prefixes = ["http://", "https://", "mailto:", "tel:"];
  return prefixes.some((it) => href.startsWith(it));
}

function A({ link, children, href, ...props }) {
  if (!href || isExternalLink(href)) {
    props.href = href;
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
      className: cx(className, extraClassName),
    };
    return React.createElement(elem, newProps, children);
  };
}

const WallOfText = wrap(
  "div",
  "wall-of-text p-4 py-10 md:py-28 leading-normal md:mx-auto md:max-w-2xl"
);
const H1 = wrap("h2", "mb-6 text-2xl font-bold text-gray-800");
const Title = wrap(H1, "text-3xl");

function Icon({ block, noshift, icon: IconComponent, ...props }) {
  return (
    <span className={block ? "block" : "inline-block"}>
      <IconComponent {...props} />
    </span>
  );
}

const FEATURES = [
  {
    name: "Code navigation",
    icon: BsCodeSlash,
    description: "Jump to definition and find all references.",
    videoId: "QRI09kTuyPQ",
  },
  {
    name: "Vim keybindings",
    description: "Full vim keybindings out of the box.",
    icon: SiVim,
    videoId: "nO_-RT8-iHM",
  },
  {
    name: "Automatic rename",
    description: "Rename any identifier along with all references to it.",
    videoId: "GPwc2vuK4sA",
    icon: ImMagicWand,
  },
  {
    name: "Postfix completions",
    description:
      "Generate code with macros that work intelligently on Go expressions.",
    icon: FaRobot,
    videoId: "qMAachk4TGI",
  },
  {
    name: "Native interface support",
    description: "Navigate and generate interfaces quickly and easily.",
    icon: FaLayerGroup,
    videoId: "CsxmGcahc4k",
  },
  {
    name: "Instant fuzzy search",
    icon: IoMdSearch,
    description: "Works on files, symbols, commands, and completions.",
    videoId: "9tl5rP97pg4",
  },
  {
    name: "Automatic import",
    description: "Pull in libraries you need without moving your cursor.",
    icon: GoPackage,
    videoId: "XrZTgAVXaQ0",
  },
  {
    name: "Parameter hints",
    icon: GrInfo,
    description: "View function signatures as you're calling them.",
    videoId: "TzJUEFedUIo",
  },
];

function YoutubeEmbed({ videoId, first }) {
  const autoplay = first ? 0 : 1;
  return (
    <div
      className="relative h-0 md:rounded-lg overflow-hidden"
      style={{ paddingBottom: "58.60%" }}
    >
      <iframe
        className="absolute t-0 l-0 w-full h-full"
        src={`https://www.youtube.com/embed/${videoId}?autoplay=${autoplay}&loop=1&playlist=${videoId}&showinfo=0&controls=1&modestbranding=1`}
        title="YouTube video player"
        frameBorder="0"
        allow="accelerometer; autoplay; clipboard-write; encrypted-media; fullscreen; gyroscope; picture-in-picture"
        allowFullScreen
      />
    </div>
  );
}

function VideoPlayer() {
  const [selected, setSelected] = React.useState(FEATURES[0]);
  const [first, setFirst] = React.useState(true);
  const isbig = useMediaQuery({ query: "(min-width: 768px)" });

  return (
    <div
      className="md:grid w-full md:max-w-screen-xl mx-auto md:gap-x-4"
      style={{
        gridTemplateColumns: "250px auto",
        ...(isbig ? { fontSize: "15px" } : {}),
      }}
    >
      <div className="max-w-full md:max-w-xs">
        {FEATURES.map((it) => {
          const isSelected = selected && selected.name === it.name;
          return (
            <button
              key={it.name}
              className={cx(
                "text-left block w-full relative overflow-hidden",
                "md:pl-1 first:mt-0 py-3 border-gray-200"
              )}
              onClick={() => {
                setFirst(false);
                setSelected(it);
              }}
            >
              <div
                className={cx(
                  "flex flex-row items-center gap-x-2",
                  isSelected ? "opacity-90" : "opacity-40"
                )}
              >
                <div className={cx("text-lg leading-none text-black")}>
                  <Icon block icon={it.icon} />
                </div>
                <div
                  className={cx(
                    "font-semibold leading-none relative",
                    isSelected ? "text-black" : "text-gray-700"
                  )}
                  style={{ paddingTop: "-1px" }}
                >
                  {it.name}
                </div>
              </div>
              <div
                className={cx(
                  "text-gray-700 overflow-hidden transition-all duration-100 ease-linear leading-snug",
                  isSelected ? "mb-2 md:mb-0 mt-1.5 max-h-20" : "mt-0 max-h-0"
                )}
              >
                {it.description}
              </div>
              {isSelected && selected.videoId && !isbig && (
                <YoutubeEmbed videoId={selected.videoId} first={first} />
              )}
            </button>
          );
        })}
      </div>
      {selected && selected.videoId && isbig && (
        <YoutubeEmbed videoId={selected.videoId} first={first} />
      )}
    </div>
  );
}

function Home() {
  return (
    <div className="mt-8 mb-14 md:my-20 w-full">
      <div className="max-w-full md:max-w-xl md:mx-auto text-lg md:text-lg leading-relaxed p-4">
        <div className="text-center font-bold text-3xl md:text-4xl mb-8 text-black leading-snug">
          A&nbsp;High&nbsp;Performance IDE&nbsp;for&nbsp;Go
        </div>
        <p>
          Written in C++, CodePerfect indexes large codebases quickly and
          responds to every key in 16ms. Supports Windows and Mac (Linux coming
          soon).
        </p>
        <p>
          Built for Vim users who want more power, Jetbrains users who want more
          speed, and everyone in between.
        </p>

        <div className="mt-8 text-center flex gap-4 justify-center">
          <A
            link
            href="/download"
            className="button main-button justify-center flex md:inline-flex text-center"
          >
            <Icon className="mr-2" icon={HiOutlineDownload} />
            Download
          </A>
          <A
            href={getLink("docs")}
            className="button download-button justify-center flex md:inline-flex text-center"
          >
            View Docs
            <Icon className="ml-2" icon={BsChevronRight} />
          </A>
        </div>
      </div>

      <div className="px-4 mt-16 md:mt-28">
        <div className="mb-8 font-bold text-black text-3xl text-center">
          A full-featured IDE, as fast as Sublime Text.
        </div>
        <VideoPlayer />
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
  "w-auto overflow-hidden border rounded-md shadow-sm"
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
  "button download-button p-3 block text-center bg-white",
  null
  // { ...disableButtonProps, href: "#" }
);

function BuyLicense() {
  return (
    <div className="max-w-screen-md px-4 mx-auto my-16 md:my-28">
      <Title>Buy CodePerfect</Title>
      <div className="my-6 grid grid-cols-1 sm:grid-cols-2 gap-6">
        <BuyLicenseBox>
          <div className="p-5">
            <h1 className="font-bold text-gray-700 mb-2">Personal</h1>
            <PricingPoint label="Commercial use ok" />
            <PricingPoint label="All features available" />
            <PricingPoint not label="Company can't pay" />
            <PricingPoint not label="Purchase can't be expensed" />
          </div>

          <div className="p-5 border-t border-gray-100 bg-gray-50">
            <div className="grid grid-cols-2 gap-3">
              <div>
                <div className="flex items-center mb-2.5">
                  <div className="leading-none text-xl font-bold text-gray-700">
                    $5
                  </div>
                  <div className="leading-none text-xs ml-1">/month</div>
                </div>
                <BuyLicenseButton href={getLink("buyPersonalMonthly")}>
                  Buy Monthly
                </BuyLicenseButton>
              </div>
              <div>
                <div className="flex items-center mb-2.5">
                  <div className="leading-none text-xl font-bold text-gray-700">
                    $50
                  </div>
                  <div className="leading-none text-xs ml-1">/year</div>
                </div>
                <BuyLicenseButton href={getLink("buyPersonalYearly")}>
                  Buy Yearly
                </BuyLicenseButton>
              </div>
            </div>
          </div>
        </BuyLicenseBox>
        <BuyLicenseBox>
          <div className="p-5">
            <h1 className="font-bold text-gray-700 mb-2">Pro</h1>
            <PricingPoint label="Commercial use ok" />
            <PricingPoint label="All features available" />
            <PricingPoint label="Company can pay" />
            <PricingPoint label="Purchase can be expensed" />
          </div>

          <div className="p-5 border-t border-gray-100 bg-gray-50">
            <div className="grid grid-cols-2 gap-3">
              <div>
                <div className="flex items-center mb-2.5">
                  <div className="leading-none text-xl font-bold text-gray-700">
                    $10
                  </div>
                  <div className="leading-none text-xs ml-1">/month</div>
                </div>
                <BuyLicenseButton href={getLink("buyProMonthly")}>
                  Buy Monthly
                </BuyLicenseButton>
              </div>
              <div>
                <div className="flex items-center mb-2.5">
                  <div className="leading-none text-xl font-bold text-gray-700">
                    $100
                  </div>
                  <div className="leading-none text-xs ml-1">/year</div>
                </div>
                <BuyLicenseButton href={getLink("buyProYearly")}>
                  Buy Yearly
                </BuyLicenseButton>
              </div>
            </div>
          </div>
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
    </div>
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
      <Title className="px-4 md:text-center text-2xl">
        Download CodePerfect {CURRENT_BUILD}
      </Title>
      <div className="max-w-3xl mx-auto mt-8 px-4 md:text-center">
        <p className="flex flex-wrap flex-col md:flex-row gap-2 justify-center">
          {links.map((it) => (
            <A
              href={
                it.disabledText
                  ? "#"
                  : `https://codeperfect95.s3.us-east-2.amazonaws.com/app/${it.platform}-${CURRENT_BUILD}.zip`
              }
              className={cx(
                "button download-button",
                it.disabledText && "disabled"
              )}
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
          className="mx-auto items-center gap-8 my-8 hidden md:flex rounded-xl overflow-hidden border border-gray-300"
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
      <H1>Terms of Service</H1>
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
        <H1>Privacy Policy</H1>
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
        not && "text-red-600"
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
  "text-black no-underline whitespace-nowrap hidden md:inline-block"
);

function Header() {
  return (
    <div className="p-4 md:p-4 border-b border-gray-100 bg-gray-50">
      <div className="flex justify-between items-center w-full md:max-w-screen-lg md:mx-auto text-lg">
        <A
          href="/"
          className="font-bold text-lg text-black no-underline whitespace-nowrap flex flex-shrink-0 items-center"
        >
          <img
            alt="logo"
            className="w-auto h-10 inline-block mr-3"
            src={asset("/logo.png")}
          />
          <span className="hidden md:inline-block logo">CodePerfect 95</span>
        </A>
        <div className="flex items-baseline space-x-6">
          <NavA href={getLink("docs")}>Docs</NavA>
          <NavA href={getLink("changelog")}>Changelog</NavA>
          <NavA href={getLink("discord")}>Discord</NavA>
          <NavA href="/buy">Buy</NavA>
          <A href="/download" className="button main-button">
            <Icon className="mr-2" icon={HiOutlineDownload} />
            Download
          </A>
        </div>
      </div>
    </div>
  );
}

const FootSection = wrap("div", "flex flex-col gap-y-3 md:gap-y-3");
const FootLink = wrap(A, "text-gray-500 no-underline");

function Footer() {
  return (
    <div className="px-4 pt-6 lg:pt-8 pb-4 md:pb-12 border-t border-gray-100 bg-gray-50">
      <div className="flex flex-col md:flex-row gap-y-4 md:gap-0 hmd:flex-row justify-between w-full md:max-w-screen-lg md:mx-auto items-start">
        <div className="flex flex-col md:flex-row md:items-start gap-y-3 md:gap-x-14 text-gray-500 leading-none">
          <span>&copy; {new Date().getFullYear()} CodePerfect 95</span>
          <FootSection>
            <FootLink href="/buy">Buy License</FootLink>
            <FootLink href="/download">Download</FootLink>
          </FootSection>
          <FootSection>
            <FootLink href={getLink("docs")}>Docs</FootLink>
            <FootLink href={getLink("changelog")}>Changelog</FootLink>
            <FootLink href={getLink("issueTracker")}>Issue Tracker</FootLink>
          </FootSection>
          <FootSection>
            <FootLink href={`mailto:${SUPPORT_EMAIL}`}>Support</FootLink>
            <FootLink href="/terms">Terms &amp; Privacy</FootLink>
            <FootLink href={LINKS.mailingList}>Newsletter</FootLink>
          </FootSection>
        </div>
        <div className="flex gap-x-4 text-2xl">
          <A className="text-gray-400" href={getLink("discord")}>
            <Icon block icon={FaDiscord} />
          </A>
          <A className="text-gray-400" href={getLink("twitter")}>
            <Icon block icon={FaTwitter} />
          </A>
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
