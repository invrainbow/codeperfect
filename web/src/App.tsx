import { Footer } from "@/components/Footer";
import { Header } from "@/components/Header";
import { Faq } from "@/pages/Faq";
import { Home } from "@/pages/Home";
import cx from "classnames";
import { Route, BrowserRouter as Router, Switch } from "react-router-dom";
import { ScrollToTop } from "./components/ScrollToTop";

export const App = () => (
  <Router>
    <ScrollToTop />
    <div className={cx("flex flex-col min-h-screen font-sans")}>
      <div className="flex-grow">
        <Header />
        <Switch>
          <Route path="/" exact component={Home} />
          <Route path="/faq" component={Faq} />
        </Switch>
      </div>
      <Footer />
    </div>
  </Router>
);
