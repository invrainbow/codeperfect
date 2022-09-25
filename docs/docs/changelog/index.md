---
title: Changelog
---

import { Redirect } from "@docusaurus/router";
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';

export default function Home() {
  const { siteConfig } = useDocusaurusContext();
  return <Redirect to={`/changelog/${siteConfig.customFields.fuckingRetardation}`} />;
};
